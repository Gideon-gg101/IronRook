#include "endgame.h"
#include "../core/bitboard.h" // For popcount
#include <algorithm>          // std::min, std::max
#include <cmath>              // std::abs

namespace Prometheus {
namespace Eval {

// Helper: Distance between squares
int distance(Square s1, Square s2) {
  if (s1 >= 64 || s2 >= 64)
    return 14;
  int r1 = s1 / 8, c1 = s1 % 8;
  int r2 = s2 / 8, c2 = s2 % 8;
  int dr = (int)r1 - (int)r2;
  int dc = (int)c1 - (int)c2;
  return std::max((dr < 0 ? -dr : dr), (dc < 0 ? -dc : dc));
}

// Distance from center
int center_dist(Square s) {
  if (s >= 64)
    return 0;
  int r = s / 8;
  int c = s % 8;
  int dr = r * 2 - 7;
  int dc = c * 2 - 7;
  return std::max((dr < 0 ? -dr : dr), (dc < 0 ? -dc : dc));
}

// Opposition detection
bool has_opposition(Square k1, Square k2) {
  if (k1 >= 64 || k2 >= 64)
    return false;
  int r1 = k1 / 8, c1 = k1 % 8;
  int r2 = k2 / 8, c2 = k2 % 8;
  int dr = std::abs(r1 - r2);
  int dc = std::abs(c1 - c2);
  // Direct opposition: distance = 2, same file or rank
  if ((dr == 2 && dc == 0) || (dr == 0 && dc == 2))
    return true;
  // Distant opposition: both dr and dc are even and non-zero
  if (dr > 0 && dc > 0 && dr % 2 == 0 && dc % 2 == 0)
    return true;
  return false;
}

// Evaluate piece trade bonuses (simplification)
int evaluate_simplification(const Board &board, int score) {
  // Count total material (pieces, not pawns)
  int w_pieces = Bitboards::popcount(board.pieces(KNIGHT, WHITE)) +
                 Bitboards::popcount(board.pieces(BISHOP, WHITE)) +
                 Bitboards::popcount(board.pieces(ROOK, WHITE)) +
                 Bitboards::popcount(board.pieces(QUEEN, WHITE));
  int b_pieces = Bitboards::popcount(board.pieces(KNIGHT, BLACK)) +
                 Bitboards::popcount(board.pieces(BISHOP, BLACK)) +
                 Bitboards::popcount(board.pieces(ROOK, BLACK)) +
                 Bitboards::popcount(board.pieces(QUEEN, BLACK));

  int w_pawns = Bitboards::popcount(board.pieces(PAWN, WHITE));
  int b_pawns = Bitboards::popcount(board.pieces(PAWN, BLACK));

  int bonus = 0;

  // If winning (score > 50cp), prefer trading pieces (not pawns)
  if (score > 50) {
    // Fewer pieces is good when winning
    int total_pieces = w_pieces + b_pieces;
    if (total_pieces <= 4)
      bonus += 20;
    if (total_pieces <= 2)
      bonus += 30;
  }
  // If losing (score < -50cp), prefer trading pawns
  else if (score < -50) {
    int total_pawns = w_pawns + b_pawns;
    if (total_pawns <= 3)
      bonus -= 20; // Negative bonus = penalty to opponent
    if (total_pawns <= 1)
      bonus -= 30;
  }

  return bonus;
}

EndgameScore evaluate_endgame(const Board &board, int current_score) {
  // Default: No scaling, no bonus
  EndgameScore es = {0, 64};

  // Check material
  Bitboard wp = board.pieces(PAWN, WHITE);
  Bitboard bp = board.pieces(PAWN, BLACK);
  Bitboard wn = board.pieces(KNIGHT, WHITE);
  Bitboard bn = board.pieces(KNIGHT, BLACK);
  Bitboard wb = board.pieces(BISHOP, WHITE);
  Bitboard bb = board.pieces(BISHOP, BLACK);
  Bitboard wr = board.pieces(ROOK, WHITE);
  Bitboard br = board.pieces(ROOK, BLACK);
  Bitboard wq = board.pieces(QUEEN, WHITE);
  Bitboard bq = board.pieces(QUEEN, BLACK);

  int w_pawns = Bitboards::popcount(wp);
  int b_pawns = Bitboards::popcount(bp);
  int w_minors = Bitboards::popcount(wn | wb);
  int b_minors = Bitboards::popcount(bn | bb);
  int w_majors = Bitboards::popcount(wr | wq);
  int b_majors = Bitboards::popcount(br | bq);

  int total_pawns = w_pawns + b_pawns;

  Bitboard wk_bb = board.pieces(KING, WHITE);
  Bitboard bk_bb = board.pieces(KING, BLACK);
  if (!wk_bb || !bk_bb)
    return es;

  Square wk_sq = Bitboards::lsb(wk_bb);
  Square bk_sq = Bitboards::lsb(bk_bb);

  // Scale down if material is low (handling drawish endings generally)
  if (w_pawns == 0 && b_pawns == 0) {
    if (w_majors == 0 && b_majors == 0) {
      // Minor piece endings without pawns
      if (w_minors <= 1 && b_minors == 0)
        es.scale_factor = 0;
      if (b_minors <= 1 && w_minors == 0)
        es.scale_factor = 0;

      // KNN vs K
      if (w_minors == 2 && b_minors == 0 && Bitboards::popcount(wn) == 2)
        es.scale_factor = 0;
      if (b_minors == 2 && w_minors == 0 && Bitboards::popcount(bn) == 2)
        es.scale_factor = 0;

      // KBN vs K (Winning)
      if (Bitboards::popcount(wn) == 1 && Bitboards::popcount(wb) == 1 &&
          b_minors == 0) {
        Square b_sq = Bitboards::lsb(wb);
        bool bishop_is_light = ((b_sq / 8) + (b_sq % 8)) % 2 != 0;

        // Target Corners logic
        int dist_a1 = distance(bk_sq, Square::SQ_A1);
        int dist_h8 = distance(bk_sq, Square::SQ_H8);
        int dist_a8 = distance(bk_sq, Square::SQ_A8);
        int dist_h1 = distance(bk_sq, Square::SQ_H1);

        int min_dist = 0;
        if (!bishop_is_light) { // Dark: A1, H8
          min_dist = std::min(dist_a1, dist_h8);
        } else { // Light: A8, H1
          min_dist = std::min(dist_a8, dist_h1);
        }

        es.score_bonus += (7 - min_dist) * 50;
        es.score_bonus += (14 - distance(wk_sq, bk_sq)) * 10;
        es.scale_factor = 64;
        return es;
      }

      // Mirror for Black KBN vs K
      if (Bitboards::popcount(bn) == 1 && Bitboards::popcount(bb) == 1 &&
          w_minors == 0) {
        Square b_sq = Bitboards::lsb(bb);
        bool bishop_is_light = ((b_sq / 8) + (b_sq % 8)) % 2 != 0;
        int dist_a1 = distance(wk_sq, Square::SQ_A1);
        int dist_h8 = distance(wk_sq, Square::SQ_H8);
        int dist_a8 = distance(wk_sq, Square::SQ_A8);
        int dist_h1 = distance(wk_sq, Square::SQ_H1);
        int min_dist = 0;
        if (!bishop_is_light) { // Dark: A1, H8
          min_dist = std::min(dist_a1, dist_h8);
        } else { // Light: A8, H1
          min_dist = std::min(dist_a8, dist_h1);
        }
        es.score_bonus -= (7 - min_dist) * 50;
        es.score_bonus -= (14 - distance(wk_sq, bk_sq)) * 10;
        es.scale_factor = 64;
        return es;
      }
    }
  }

  // --- KBPK: Wrong Colored Bishop ---
  if (total_pawns == 1 && b_minors == 0 && b_majors == 0 && w_minors == 1) {
    if (w_pawns == 1 && Bitboards::popcount(wb) == 1) {
      Square p_sq = Bitboards::lsb(wp);
      int file = p_sq % 8;
      if (file == 0 || file == 7) { // Rook Pawn
        Square b_sq = Bitboards::lsb(wb);
        bool bishop_light = ((b_sq / 8) + (b_sq % 8)) % 2 != 0;
        bool prom_sq_light = (file == 0) ? true : false;
        if (bishop_light != prom_sq_light) {
          Square prom_sq = (file == 0) ? Square::SQ_A8 : Square::SQ_H8;
          if (distance(bk_sq, prom_sq) <= 2) {
            es.scale_factor = 0; // DRAW
            return es;
          }
        }
      }
    }
  }
  // Mirror for Black
  if (total_pawns == 1 && w_minors == 0 && w_majors == 0 && b_minors == 1) {
    if (b_pawns == 1 && Bitboards::popcount(bb) == 1) {
      Square p_sq = Bitboards::lsb(bp);
      int file = p_sq % 8;
      if (file == 0 || file == 7) {
        Square b_sq = Bitboards::lsb(bb);
        bool bishop_light = ((b_sq / 8) + (b_sq % 8)) % 2 != 0;
        bool prom_sq_light = (file == 0) ? false : true;
        if (bishop_light != prom_sq_light) {
          Square prom_sq = (file == 0) ? Square::SQ_A1 : Square::SQ_H1;
          if (distance(wk_sq, prom_sq) <= 2) {
            es.scale_factor = 0;
            return es;
          }
        }
      }
    }
  }

  // --- RPKR: Rook+Pawn vs Rook (Fortress Detection) ---
  if (total_pawns == 1 && w_majors == 1 && b_majors == 1 && w_minors == 0 &&
      b_minors == 0) {
    if (Bitboards::popcount(wr) == 1 && Bitboards::popcount(br) == 1) {
      // White Pawn
      if (w_pawns == 1) {
        Square p_sq = Bitboards::lsb(wp);
        int file = p_sq % 8;
        int rank = p_sq / 8;
        int k_file = bk_sq % 8;
        int k_rank = bk_sq / 8;
        if (k_file == file && k_rank > rank) {
          es.scale_factor = 0;
          return es;
        }
      }
      // Black Pawn
      else if (b_pawns == 1) {
        Square p_sq = Bitboards::lsb(bp);
        int file = p_sq % 8;
        int rank = p_sq / 8;
        int k_file = wk_sq % 8;
        int k_rank = wk_sq / 8;
        if (k_file == file && k_rank < rank) {
          es.scale_factor = 0;
          return es;
        }
      }
    }
  }

  // --- KPK: Rook Pawn Draw ---
  if (total_pawns == 1 && w_minors == 0 && b_minors == 0 && w_majors == 0 &&
      b_majors == 0) {
    if (w_pawns == 1) {
      Square p_sq = Bitboards::lsb(wp);
      int file = p_sq % 8;
      if (file == 0 || file == 7) {
        int p_rank = p_sq / 8;
        int k_rank = bk_sq / 8;
        int k_file = bk_sq % 8;
        if (k_file == file && k_rank > p_rank) {
          es.scale_factor = 0;
          return es;
        }
      }
    } else if (b_pawns == 1) {
      Square p_sq = Bitboards::lsb(bp);
      int file = p_sq % 8;
      if (file == 0 || file == 7) {
        int p_rank = p_sq / 8;
        int k_rank = wk_sq / 8;
        int k_file = wk_sq % 8;
        if (k_file == file && k_rank < p_rank) {
          es.scale_factor = 0;
          return es;
        }
      }
    }
  }

  // Opposite Colored Bishops (OCB)
  if (w_minors == 1 && b_minors == 1 && w_majors == 0 && b_majors == 0) {
    if (Bitboards::popcount(wb) == 1 && Bitboards::popcount(bb) == 1) {
      Square w_sq = Bitboards::lsb(wb);
      Square b_sq = Bitboards::lsb(bb);
      bool w_light = ((w_sq / 8) + (w_sq % 8)) % 2 != 0;
      bool b_light = ((b_sq / 8) + (b_sq % 8)) % 2 != 0;
      if (w_light != b_light) {
        if (total_pawns == 0)
          es.scale_factor = 0;
        else if (total_pawns <= 2)
          es.scale_factor = 16;
        else
          es.scale_factor = 32;
      }
    }
  }

  // Opposition in Pawn Endgames
  if (total_pawns > 0 && w_majors == 0 && b_majors == 0 && w_minors == 0 &&
      b_minors == 0) {
    // Pure pawn endgame
    bool white_has_opp = has_opposition(wk_sq, bk_sq);
    if (white_has_opp) {
      es.score_bonus += 30; // White benefits from opposition
    } else {
      bool black_has_opp = has_opposition(bk_sq, wk_sq);
      if (black_has_opp) {
        es.score_bonus -= 30; // Black benefits
      }
    }
  }

  // King Activity: Support for passed pawns
  // Check if king is in front of or supporting own passed pawns
  Bitboard white_passers =
      wp; // Simplified: assume all pawns are potential passers
  Bitboard black_passers = bp;

  while (white_passers) {
    Square p_sq = Bitboards::pop_lsb(white_passers);
    int p_rank = p_sq / 8;
    int p_file = p_sq % 8;
    int k_rank = wk_sq / 8;
    int k_file = wk_sq % 8;

    // King in front of pawn (supporting)
    if (k_file == p_file && k_rank > p_rank) {
      es.score_bonus += 20;
    }
    // King close to pawn
    if (distance(wk_sq, p_sq) <= 2) {
      es.score_bonus += 10;
    }
  }

  while (black_passers) {
    Square p_sq = Bitboards::pop_lsb(black_passers);
    int p_rank = p_sq / 8;
    int p_file = p_sq % 8;
    int k_rank = bk_sq / 8;
    int k_file = bk_sq % 8;

    if (k_file == p_file && k_rank < p_rank) {
      es.score_bonus -= 20;
    }
    if (distance(bk_sq, p_sq) <= 2) {
      es.score_bonus -= 10;
    }
  }

  // Simplification logic
  es.score_bonus += evaluate_simplification(board, current_score);

  // General Material Scaling
  if (total_pawns == 0) {
    if (w_majors == 0 && b_majors == 0 && std::abs(w_minors - b_minors) <= 1) {
      es.scale_factor = es.scale_factor * 16 / 64;
    } else {
      es.scale_factor = es.scale_factor * 32 / 64;
    }
  } else if (total_pawns <= 2) {
    es.scale_factor = es.scale_factor * 48 / 64;
  }

  return es;
}

} // namespace Eval
} // namespace Prometheus
