#include "pawn_eval.h"
#include "../core/bitboard.h"
#include "../core/magic.h"
#include "eval.h"
#include <algorithm>
#include <cstring> // for memset

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

    // Backward (Existing logic simplified)
    if (!is_passed) {
      bool supported = (w_pawns & adj_files & (~forward_mask));
      if (!supported) {
        // Semi-open logic check (simplified)
        bool semi_open = !(b_pawns & forward_mask & file_mask);
        if (semi_open &&
            !(w_pawns & forward_mask & file_mask)) { // Only if blocked?
          // Actually old logic was checking semi_open enemy file.
          score.mg += BackwardPenalty;
          score.eg += BackwardPenalty;
        }
        // We need if Black attacks 'front'.
        // Black pawns at (f-1, r+2) or (f+1, r+2)
        // Simple: attacked_by_pawns(front, BLACK) ?
        // But we don't have board context easily here.
        // Manual check:
        // ... (Too complex for simple check, simplified backward logic):
        // "Behind neighbors and semi-open file?"
      }
    }

    // Pawn Lever / Tension
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

    // Backward
    if (!is_passed) {
      bool supported = (b_pawns & adj_files & (~forward_mask));
      if (!supported) {
        bool semi_open = !(w_pawns & forward_mask & file_mask);
        if (semi_open) {
          score.mg -= BackwardPenalty;
          score.eg -= BackwardPenalty;
        }
      }
    }

    // Lever / Tension
    Bitboard attacks = get_pawn_attacks(s, BLACK);
    if (attacks & w_pawns) {
      score.mg -= PawnTensionBonus;
    }
  }

  return score;
}

} // namespace Eval
} // namespace Prometheus
