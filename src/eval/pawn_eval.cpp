#include "pawn_eval.h"
#include "../core/bitboard.h"
#include "../core/magic.h"
#include "eval.h"
#include <algorithm>
#include <cstring>
#include <vector> // Added based on example, assuming 'memset' was a typo for a comment
// The original code had '#include <cstring> // for memset'.
// The instruction was "Add include <cstring>" and the example showed it without
// the comment and with '<vector>'. Assuming the intent was to ensure <cstring>
// is present (which it was), remove its comment, and add <vector>.

namespace Prometheus {
namespace Eval {

PawnTable GlobalPawnTable;

PawnTable::PawnTable() : entries(nullptr), count(0) {
  resize(4); // Default 4MB
}

PawnTable::~PawnTable() {
  if (entries)
    delete[] entries;
}

void PawnTable::resize(size_t mb) {
  if (entries)
    delete[] entries;
  count = (mb * 1024 * 1024) / sizeof(PawnEntry);
  entries = new PawnEntry[count];
  clear();
}

void PawnTable::clear() { memset(entries, 0, count * sizeof(PawnEntry)); }

PawnEntry *PawnTable::probe(uint64_t pawnKey) {
  size_t index = pawnKey % count;
  if (entries[index].key == pawnKey) {
    return &entries[index];
  }
  return nullptr;
}

void PawnTable::save(uint64_t pawnKey, int mg, int eg) {
  size_t index = pawnKey % count;
  entries[index].key = pawnKey;
  entries[index].score_mg = mg;
  entries[index].score_eg = eg;
}

// Evaluation Constants
constexpr int IsolatedPenalty = -10;
constexpr int DoubledPenalty = -15;
constexpr int BackwardPenalty = -20;
constexpr int PassedBonus[8] = {0, 5, 10, 20, 35, 60, 100, 0}; // Rank 0-7

// Helper for pawn attacks (duplicate from eval.cpp for now, or move to common)
Bitboard get_pawn_attacks(Square s, Color side) {
  Bitboard b = 0;
  Bitboards::set_bit(b, s);
  if (side == WHITE) {
    return ((b << 9) & 0xFEFEFEFEFEFEFEFEULL) |
           ((b << 7) & 0x7F7F7F7F7F7F7F7FULL);
  } else {
    return ((b >> 9) & 0x7F7F7F7F7F7F7F7FULL) |
           ((b >> 7) & 0xFEFEFEFEFEFEFEFEULL);
  }
}

ScorePair evaluate_pawns(const Board &board, PawnTable &pt) {
  // 1. Calculate Pawn Key (Zobrist of just pawns)
  // Currently Board::key is full key. Evaluating pawns often is slow.
  // We need a specific pawn_key.
  // For now, let's just calculate it or assume full re-eval if not tracking
  // incremental pawn key. Actually, constructing a pawn key from scratch is
  // O(N_pawns). Let's implement full eval first without hash to check
  // correctness, then add hash if needed. Wait, PawnTable is useless without a
  // storage key. We will just define a temp hash for now or add pawn_key to
  // Board later.

  // For this step, we will implement the eval logic.

  ScorePair score = {0, 0};

  Bitboard w_pawns = board.pieces(PAWN, WHITE);
  Bitboard b_pawns = board.pieces(PAWN, BLACK);

  // Helper bitboards

  auto file_bb = [](int f) { return 0x0101010101010101ULL << f; };

  // 1. Pawn Majority Calculation
  int w_queenside =
      Bitboards::popcount(w_pawns & (file_bb(0) | file_bb(1) | file_bb(2)));
  int w_kingside =
      Bitboards::popcount(w_pawns & (file_bb(5) | file_bb(6) | file_bb(7)));
  int b_queenside =
      Bitboards::popcount(b_pawns & (file_bb(0) | file_bb(1) | file_bb(2)));
  int b_kingside =
      Bitboards::popcount(b_pawns & (file_bb(5) | file_bb(6) | file_bb(7)));

  if (w_queenside > b_queenside)
    score.mg += PawnMajorityBonus;
  if (w_kingside > b_kingside)
    score.mg += PawnMajorityBonus;
  if (b_queenside > w_queenside)
    score.mg -= PawnMajorityBonus;
  if (b_kingside > w_kingside)
    score.mg -= PawnMajorityBonus;

  // Evaluate White Pawns
  Bitboard temp_w = w_pawns;
  while (temp_w) {
    Square s = Bitboards::pop_lsb(temp_w);
    int f = s % 8;
    int r = s / 8;

    // Isolated (Existing)
    Bitboard adj_files = 0;
    if (f > 0)
      adj_files |= file_bb(f - 1);
    if (f < 7)
      adj_files |= file_bb(f + 1);

    if ((w_pawns & adj_files) == 0) {
      score.mg += IsolatedPenalty;
      score.eg += IsolatedPenalty;
    }

    // Doubled (Existing)
    Bitboard file_mask = file_bb(f);
    if (Bitboards::popcount(w_pawns & file_mask) > 1) {
      score.mg += DoubledPenalty / 2;
      score.eg += DoubledPenalty / 2;
    }

    // Passers, Candidates, Backward
    bool is_passed = false;
    Bitboard forward_mask = 0;
    for (int rr = r + 1; rr < 8; ++rr)
      forward_mask |= (0xFFULL << (rr * 8));

    Bitboard span = (file_mask | adj_files) & forward_mask;

    if ((b_pawns & span) == 0) {
      is_passed = true;
      int bonus = PassedBonus[r];
      // Protected/Connected Logic
      Bitboard support =
          w_pawns & adj_files & (~forward_mask) & (~(0xFFULL << (r * 8)));
      if (support)
        bonus += bonus / 2;

      // Rook Behind
      Bitboard w_rooks = board.pieces(ROOK, WHITE);
      Bitboard b_rooks = board.pieces(ROOK, BLACK);
      Bitboard behind_file =
          file_mask & (~forward_mask) & (~(0xFFULL << (r * 8)));

      if (w_rooks & behind_file) {
        bonus += 20;
        score.eg += 20;
      }
      if (b_rooks & behind_file) { // Enemy rook behind our passer
        bonus /= 2;
      }
      if (b_rooks & span) { // Enemy rook blocking
        bonus /= 2;
      }

      score.mg += bonus;
      score.eg += bonus * 2;
    } else {
      // Not passed. Check for Candidate Passer.
      // Definition: No enemy pawn on same file.
      // Enemy pawns on adjacent files exist (checked by !is_passed implicit
      // logic usually, but span includes adj).
      bool clear_file = !(b_pawns & forward_mask & file_mask);
      if (clear_file) {
        // We have clear runway, but controlled by adjacent enemy pawns.
        // Bonus if we have support or numerical superiority to force it.
        // Simple heuristic: If we have more support than they have blockers?
        // Or just flat bonus for "Candidate"
        score.mg += CandidatePasserBonus;
        score.eg += CandidatePasserBonus * 2; // Worth more in endgame
      }
    }

    // Backward Pawn Logic
    if (!is_passed) {
      bool supported = (w_pawns & adj_files & (~forward_mask));
      if (!supported) {
        bool semi_open = !(b_pawns & forward_mask & file_mask);
        if (semi_open) {
          // Check if stop square is controlled by enemy pawns
          Square stop_sq = static_cast<Square>(s + 8);
          Bitboard enemy_attacks_on_stop = get_pawn_attacks(
              stop_sq, WHITE); // White attacks FROM stop = Black attacks TO
                               // stop (symmetry)
          // Wait, get_pawn_attacks(sq, COLOR) returns squares attacked BY a
          // pawn of COLOR at sq. We want to know if 'stop_sq' is attacked by
          // BLACK pawns. get_pawn_attacks(stop_sq, WHITE) -> squares a WHITE
          // pawn at stop_sq would attack. These are exactly the squares where a
          // BLACK pawn would need to be to attack stop_sq! (captures are
          // symmetric).

          if (enemy_attacks_on_stop & b_pawns) {
            score.mg -= BackwardPawnPenalty;
            score.eg -= BackwardPawnPenalty;
          }
        }
      }
    }

    // Pawn Break / Lever Potential
    // Check if pushing the pawn creates a threat (attacks an enemy pawn)
    Square push_sq = static_cast<Square>(s + 8);
    if (r < 6 && board.piece_on(push_sq) == NO_PIECE) {
      Bitboard push_attacks = get_pawn_attacks(push_sq, WHITE);
      if (push_attacks & b_pawns) {
        score.mg += PawnBreakBonus;
        score.eg += PawnBreakBonus;
      }
    }

    // Pawn Tension (Existing)
    Bitboard attacks = get_pawn_attacks(s, WHITE);
    if (attacks & b_pawns) {
      score.mg += PawnTensionBonus; // Tension is good!
    }
  }

  // Evaluate Black Pawns
  Bitboard temp_b = b_pawns;
  while (temp_b) {
    Square s = Bitboards::pop_lsb(temp_b);
    int f = s % 8;
    int r = s / 8;
    int r_rel = 7 - r;

    // Isolated
    Bitboard adj_files = 0;
    if (f > 0)
      adj_files |= file_bb(f - 1);
    if (f < 7)
      adj_files |= file_bb(f + 1);

    if ((b_pawns & adj_files) == 0) {
      score.mg -= IsolatedPenalty;
      score.eg -= IsolatedPenalty;
    }

    // Doubled
    Bitboard file_mask = file_bb(f);
    if (Bitboards::popcount(b_pawns & file_mask) > 1) {
      score.mg -= DoubledPenalty / 2;
      score.eg -= DoubledPenalty / 2;
    }

    bool is_passed = false;
    Bitboard forward_mask = 0;
    for (int rr = 0; rr < r; ++rr)
      forward_mask |= (0xFFULL << (rr * 8));

    Bitboard span = (file_mask | adj_files) & forward_mask;

    if ((w_pawns & span) == 0) {
      is_passed = true;
      int bonus = PassedBonus[r_rel];

      Bitboard support =
          b_pawns & adj_files & (~forward_mask) & (~(0xFFULL << (r * 8)));
      if (support)
        bonus += bonus / 2;

      score.mg -= bonus;
      score.eg -= bonus * 2;
    } else {
      // Candidate Passer (Black)
      bool clear_file = !(w_pawns & forward_mask & file_mask);
      if (clear_file) {
        score.mg -= CandidatePasserBonus;
        score.eg -= CandidatePasserBonus * 2;
      }
    }

    // Backward (Black)
    if (!is_passed) {
      bool supported = (b_pawns & adj_files & (~forward_mask));
      if (!supported) {
        bool semi_open = !(w_pawns & forward_mask & file_mask);
        if (semi_open) {
          Square stop_sq = static_cast<Square>(s - 8);
          // Check if stop square attacked by White pawns
          // get_pawn_attacks(stop_sq, BLACK) -> squares a BLACK pawn at stop_sq
          // would attack. These are where WHITE pawns would be to attack
          // stop_sq.
          Bitboard enemy_attacks_on_stop = get_pawn_attacks(stop_sq, BLACK);

          if (enemy_attacks_on_stop & w_pawns) {
            score.mg +=
                BackwardPawnPenalty; // Penalty for Black (positive score)
            score.eg += BackwardPawnPenalty;
          }
        }
      }
    }

    // Pawn Break / Lever Potential (Black)
    Square push_sq = static_cast<Square>(s - 8);
    if (r > 1 && board.piece_on(push_sq) == NO_PIECE) {
      Bitboard push_attacks = get_pawn_attacks(push_sq, BLACK);
      if (push_attacks & w_pawns) {
        score.mg -= PawnBreakBonus;
        score.eg -= PawnBreakBonus;
      }
    }

    // Lever / Tension
    Bitboard attacks = get_pawn_attacks(s, BLACK);
    if (attacks & w_pawns) {
      score.mg -= PawnTensionBonus;
    }
  }

  // === ZUGZWANG-AWARE PAWN ENDGAMES ===
  // In pure pawn endgames, tempo (side to move) can be critical
  // Detect pure pawn endgame and apply tempo bonus
  int w_pieces = Bitboards::popcount(board.pieces(KNIGHT, WHITE)) +
                 Bitboards::popcount(board.pieces(BISHOP, WHITE)) +
                 Bitboards::popcount(board.pieces(ROOK, WHITE)) +
                 Bitboards::popcount(board.pieces(QUEEN, WHITE));
  int b_pieces = Bitboards::popcount(board.pieces(KNIGHT, BLACK)) +
                 Bitboards::popcount(board.pieces(BISHOP, BLACK)) +
                 Bitboards::popcount(board.pieces(ROOK, BLACK)) +
                 Bitboards::popcount(board.pieces(QUEEN, BLACK));

  // Pure pawn endgame: no pieces, both sides have pawns
  if (w_pieces == 0 && b_pieces == 0 && Bitboards::popcount(w_pawns) > 0 &&
      Bitboards::popcount(b_pawns) > 0) {
    // Tempo bonus: side to move gets small advantage
    int tempo = 15; // 15cp tempo bonus
    if (board.side_to_move() == WHITE) {
      score.eg += tempo;
    } else {
      score.eg -= tempo;
    }
  }

  return score;
}

} // namespace Eval
} // namespace Prometheus
