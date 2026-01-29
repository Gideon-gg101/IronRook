#include "endgame.h"
#include "../core/bitboard.h" // For popcount
#include <algorithm>          // std::min, std::max
#include <cmath>              // std::abs

namespace Prometheus {
namespace Eval {

// Helper: Distance between squares
int distance(Square s1, Square s2) {
  int r1 = s1 / 8, c1 = s1 % 8;
  int r2 = s2 / 8, c2 = s2 % 8;
  int dr = (int)r1 - (int)r2;
  int dc = (int)c1 - (int)c2;
  return std::max((dr < 0 ? -dr : dr), (dc < 0 ? -dc : dc));
}

// Distance from center
int center_dist(Square s) {
  int r = s / 8;
  int c = s % 8;
  int dr = r * 2 - 7;
  int dc = c * 2 - 7;
  return std::max((dr < 0 ? -dr : dr), (dc < 0 ? -dc : dc));
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

  // Scale down if material is low (handling drawish endings generally)
  if (w_pawns == 0 && b_pawns == 0) {
    if (w_majors == 0 && b_majors == 0) {
      // Minor piece endings without pawns
      if (w_minors <= 1 && b_minors == 0)
        es.scale_factor = 0;
      if (b_minors <= 1 && w_minors == 0)
        es.scale_factor = 0;

      // KNN vs K
      if (w_minors == 2 && b_minors == 0 && w_minors == Bitboards::popcount(wn))
        es.scale_factor = 0;
      if (b_minors == 2 && w_minors == 0 && b_minors == Bitboards::popcount(bn))
        es.scale_factor = 0;

      // KBN vs K (Winning)
      if (Bitboards::popcount(wn) == 1 && Bitboards::popcount(wb) == 1 &&
          b_minors == 0) {
        // White KBN vs Black K
        Square b_sq = Bitboards::lsb(wb);
        Square bk_sq = Bitboards::lsb(board.pieces(KING, BLACK));

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
        Square wk_sq = Bitboards::lsb(board.pieces(KING, WHITE));
        es.score_bonus += (14 - distance(wk_sq, bk_sq)) * 10;
        es.scale_factor = 64;
        return es;
      }

      // Mirror for Black KBN vs K
      if (Bitboards::popcount(bn) == 1 && Bitboards::popcount(bb) == 1 &&
          w_minors == 0) {
        Square b_sq = Bitboards::lsb(bb);
        Square wk_sq = Bitboards::lsb(board.pieces(KING, WHITE));
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
        Square bk_sq = Bitboards::lsb(board.pieces(KING, BLACK));
        es.score_bonus -= (14 - distance(wk_sq, bk_sq)) * 10;
        es.scale_factor = 64;
        return es;
      }
    }
  }

  // --- KBPK: Wrong Colored Bishop ---
  if (total_pawns == 1 && b_minors == 0 && b_majors == 0) {
    if (w_minors == 1 && Bitboards::popcount(wb) == 1) {
      // White KBP vs Black K
      Square p_sq = Bitboards::lsb(wp);
      int file = p_sq % 8;
      if (file == 0 || file == 7) { // Rook Pawn
        Square b_sq = Bitboards::lsb(wb);
        bool bishop_light = ((b_sq / 8) + (b_sq % 8)) % 2 != 0;
        bool prom_sq_light =
            (file == 0) ? true
                        : false; // A8(Light), H8(Dark) Check: A8 is Light.

        if (bishop_light != prom_sq_light) {
          Square bk_sq = Bitboards::lsb(board.pieces(KING, BLACK));
          Square prom_sq = (file == 0) ? Square::SQ_A8 : Square::SQ_H8;
          if (distance(bk_sq, prom_sq) <= 2) {
            es.scale_factor = 0; // DRAW
            return es;
          }
        }
      }
    }
    // Mirror for Black
    if (b_minors == 1 && Bitboards::popcount(bb) == 1) {
      Square p_sq = Bitboards::lsb(bp);
      int file = p_sq % 8;
      if (file == 0 || file == 7) {
        Square b_sq = Bitboards::lsb(bb);
        bool bishop_light = ((b_sq / 8) + (b_sq % 8)) % 2 != 0;
        bool prom_sq_light = (file == 0) ? false : true; // A1(Dark), H1(Light)

        if (bishop_light != prom_sq_light) {
          Square wk_sq = Bitboards::lsb(board.pieces(KING, WHITE));
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
        // Square bk_sq = Bitboards::lsb(board.pieces(KING, BLACK)); // Unused
        Square bk_sq = Bitboards::lsb(board.pieces(KING, BLACK));
        int file = p_sq % 8;
        int rank = p_sq / 8;
        int k_file = bk_sq % 8;
        int k_rank = bk_sq / 8;

        bool draw = false;
        if (k_file == file && k_rank > rank) {
          draw = true;
        }

        if (draw) {
          es.scale_factor = 0; // Theoretical Draw / Fortress
          return es;
        }
      }
      // Black Pawn
      else {
        Square p_sq = Bitboards::lsb(bp);
        Square wk_sq = Bitboards::lsb(board.pieces(KING, WHITE));
        int file = p_sq % 8;
        int rank = p_sq / 8;
        int k_file = wk_sq % 8;
        int k_rank = wk_sq / 8;

        bool draw = false;
        // King on same file, in front (rank < p_rank)
        if (k_file == file && k_rank < rank) {
          draw = true;
        }

        if (draw) {
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
        Square bk_sq = Bitboards::lsb(board.pieces(KING, BLACK));
        int p_rank = p_sq / 8;
        int k_rank = bk_sq / 8;
        int k_file = bk_sq % 8;
        if (k_file == file && k_rank > p_rank) {
          es.scale_factor = 0;
          return es;
        }
      }
    } else { // Black Pawn
      Square p_sq = Bitboards::lsb(bp);
      int file = p_sq % 8;
      if (file == 0 || file == 7) {
        Square wk_sq = Bitboards::lsb(board.pieces(KING, WHITE));
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
