#include "pawn_eval.h"
#include "../core/bitboard.h"
#include "../core/magic.h"
#include <algorithm>
#include <cstring> // for memset

namespace IroonRook {
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

void PawnTable::clear() { std::memset(entries, 0, count * sizeof(PawnEntry)); }

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

  Bitboard all_pawns = w_pawns | b_pawns;

  // Helper bitboards
  auto file_bb = [](int f) { return 0x0101010101010101ULL << f; };

  // Evaluate White Pawns
  Bitboard temp_w = w_pawns;
  while (temp_w) {
    Square s = Bitboards::pop_lsb(temp_w);
    int f = s % 8;
    int r = s / 8;

    // Isolated
    Bitboard adj_files = 0;
    if (f > 0)
      adj_files |= file_bb(f - 1);
    if (f < 7)
      adj_files |= file_bb(f + 1);

    if ((w_pawns & adj_files) == 0) {
      score.mg += IsolatedPenalty;
      score.eg += IsolatedPenalty;
    }

    // Doubled
    Bitboard file_mask = file_bb(f);
    if (Bitboards::popcount(w_pawns & file_mask) > 1) {
      // Penalize each pawn? Or just once per file? usually each.
      score.mg += DoubledPenalty / 2; // Split penalty
      score.eg += DoubledPenalty / 2;
    }

    // Passers, Backward, Levers
    bool is_passed = false;

    // Passed Logic
    // No enemy pawns on same file or adjacent files *in front*
    Bitboard forward_mask = 0;
    // All ranks ahead of r
    for (int rr = r + 1; rr < 8; ++rr)
      forward_mask |= (0xFFULL << (rr * 8));

    Bitboard span = (file_mask | adj_files) & forward_mask;

    if ((b_pawns & span) == 0) {
      is_passed = true;
      int bonus = PassedBonus[r];
      // Protected passed pawn?
      if (w_pawns & adj_files &
          (file_bb(f - 1) |
           file_bb(f + 1))) // Neighbors (simplified, check rank)
      {
        // Actually check if supported by pawn behind?
        // Simply: "Connected" passed pawn.
        Bitboard support =
            w_pawns & adj_files & (~forward_mask) & (~(0xFFULL << (r * 8)));
        if (support)
          bonus += bonus / 2;
      }

      // Rook Behind Passer
      Bitboard w_rooks = board.pieces(ROOK, WHITE);
      Bitboard b_rooks = board.pieces(ROOK, BLACK);
      Bitboard behind_file =
          file_mask & (~forward_mask) & (~(0xFFULL << (r * 8)));
      if (w_rooks & behind_file) {
        bonus += 20; // Friendly rook behind
        score.eg += 20;
      }
      if (b_rooks & behind_file) {
        bonus -= 10;
        bonus -= 20;
      }
      // If Enemy rook is IN FRONT (blocking)?
      if (b_rooks & span) {
        bonus /= 2; // Passers blocked by rooks are weak
      }

      score.mg += bonus;
      score.eg += bonus * 2;
    }

    // Backward Pawn
    // 1. Not passed.
    // 2. Cannot advance safely (controlled by enemy sentry pawn).
    // 3. No friendly pawn support (e.g. no pawn on adjacent file behind or same
    // rank).
    if (!is_passed) {
      bool supported =
          (w_pawns & adj_files & (~forward_mask)); // Pawns behind or equal
      if (!supported) {
        // Check if advance is blocked or controlled
        // Advance square: s + 8
        Square front = Square(s + 8);
        // Controlled by enemy pawn?
        // Enemy pawns attacking 'front': front+7, front+9 ?
        // i.e. r+2.
        if (r < 6) {
          // Pseudo attacks to 'front' from Black
          // Black attacks SouthWest/SouthEast.
          // Attackers to 'front': front+7, front+9 ?
          // Check if B pawn exists that attacks 'front'.
          Bitboard b_attacks = get_pawn_attacks(
              front, WHITE); // Attacks FROM front (White) -> No.
          // We need if Black attacks 'front'.
          // Black pawns at (f-1, r+2) or (f+1, r+2)
          // Simple: attacked_by_pawns(front, BLACK) ?
          // But we don't have board context easily here.
          // Manual check:
          Bitboard enemies = b_pawns;
          // ... (Too complex for simple check, simplified backward logic):
          // "Behind neighbors and semi-open file?"
          bool semi_open = !(b_pawns & forward_mask & file_mask);
          if (semi_open) {
            score.mg += BackwardPenalty;
            score.eg += BackwardPenalty;
          }
        }
      }
    }

    // Pawn Lever (can capture)
    // Check if s captures any enemy pawn
    Bitboard attacks = get_pawn_attacks(s, WHITE);
    if (attacks & b_pawns) {
      score.mg += 10; // Lever bonus
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
      forward_mask |= (0xFFULL << (rr * 8)); // Ranks 0 to r-1

    Bitboard span = (file_mask | adj_files) & forward_mask;

    if ((w_pawns & span) == 0) {
      is_passed = true;
      int bonus = PassedBonus[r_rel];

      // Connected passed?
      Bitboard support =
          b_pawns & adj_files & (~forward_mask) & (~(0xFFULL << (r * 8)));
      if (support)
        bonus += bonus / 2;

      score.mg -= bonus;
      score.eg -= bonus * 2;
    }

    // Backward (Simplified)
    if (!is_passed) {
      // No support
      bool supported =
          (b_pawns & adj_files &
           (~forward_mask)); // "Behind" means higher rank index? No,
                             // Black moves down. Behind is r+1..7.
      // Wait, forward_mask is 0..r-1 (ahead).
      // support is in ~forward_mask (r..7).
      if (!supported) {
        // Semi-open file ahead?
        bool semi_open = !(w_pawns & forward_mask & file_mask);
        if (semi_open) {
          score.mg -= BackwardPenalty;
          score.eg -= BackwardPenalty;
        }
      }
    }

    // Lever
    Bitboard attacks = get_pawn_attacks(s, BLACK);
    if (attacks & w_pawns) {
      score.mg -= 10;
    }
  }

  return score;
}

} // namespace Eval
} // namespace IroonRook
