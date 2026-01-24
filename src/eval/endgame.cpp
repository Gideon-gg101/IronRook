#include "endgame.h"
#include "../core/bitboard.h" // For popcount
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

// Distance from center (Manhattan-ish or Chebychev from e4/d4/e5/d5)
// Center distance: max(dist(r, 3.5), dist(c, 3.5))
// We can use pre-computed simple array.
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

  // Scale down if material is low (handling drawish endings generally)
  if (w_pawns == 0 && b_pawns == 0) {
    if (w_majors == 0 && b_majors == 0) {
      // Minor piece endings without pawns
      // KN vs K, KB vs K -> Draw (Scale 0) except if we want to flag it?
      // current_score might be high due to material, but we need 0.
      if (w_minors <= 1 && b_minors == 0)
        es.scale_factor = 0;
      if (b_minors <= 1 && w_minors == 0)
        es.scale_factor = 0;

      // KNN vs K is draw (theoretically)
      if (w_minors == 2 && b_minors == 0 && w_minors == Bitboards::popcount(wn))
        es.scale_factor = 0; // 2 Knights
      if (b_minors == 2 && w_minors == 0 && b_minors == Bitboards::popcount(bn))
        es.scale_factor = 0;

      // KBN vs K (Winning)
      // Need to push King to corner of Bishop color.
      if (Bitboards::popcount(wn) == 1 && Bitboards::popcount(wb) == 1 &&
          b_minors == 0) {
        // White KBN vs Black K
        Square b_sq = Bitboards::lsb(wb);
        Square bk_sq = Bitboards::lsb(board.pieces(KING, BLACK));

        // Color of bishop square
        bool bishop_is_light = ((b_sq / 8) + (b_sq % 8)) % 2 != 0;

        // Target Corners:
        // Light B: H1 (7), A8 (56). Dark B: A1 (0), H8 (63).
        // Distance to wrong corner?

        // Simplified: Push enemy king to ANY corner? No, must be specific.
        // Bonus for distance to correct corner.
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

        // Invert distance: Closer is better. Max dist is 7.
        es.score_bonus += (7 - min_dist) * 50; // Huge bonus to drive mate

        // Also drive kings together
        Square wk_sq = Bitboards::lsb(board.pieces(KING, WHITE));
        es.score_bonus += (14 - distance(wk_sq, bk_sq)) * 10;

        es.scale_factor = 64; // Keep full evaluation
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

  // Opposite Colored Bishops (OCB) preservation
  // If 1 Bishop each, no other pieces (except pawns)
  // Check if opposite color
  if (w_minors == 1 && b_minors == 1 && w_majors == 0 && b_majors == 0) {
    if (Bitboards::popcount(wb) == 1 && Bitboards::popcount(bb) == 1) {
      Square w_sq = Bitboards::lsb(wb);
      Square b_sq = Bitboards::lsb(bb);
      bool w_light = ((w_sq / 8) + (w_sq % 8)) % 2 != 0;
      bool b_light = ((b_sq / 8) + (b_sq % 8)) % 2 != 0;
      if (w_light != b_light) {
        // OCB
        // Scale down based on number of pawns.
        // 1 pawn vs 0 -> Drawish.
        // 2 vs 1 -> Drawish.
        // Symmetric pawns -> Very Drawish.
        int total_pawns = w_pawns + b_pawns;
        if (total_pawns == 0)
          es.scale_factor = 0; // Pure OCB is draw
        else if (total_pawns <= 2)
          es.scale_factor = 16; // Very drawish
        else
          es.scale_factor = 32; // Still drawish
      }
    }
  }

  // General Material Scaling: Dampen scores if few pawns exist
  int total_pawns = w_pawns + b_pawns;
  if (total_pawns == 0) {
    if (w_majors == 0 && b_majors == 0 && std::abs(w_minors - b_minors) <= 1) {
      es.scale_factor =
          es.scale_factor * 16 / 64; // Heavily dampen pawnless minor endings
    } else {
      es.scale_factor =
          es.scale_factor * 32 / 64; // Dampen other pawnless endings
    }
  } else if (total_pawns <= 2) {
    es.scale_factor =
        es.scale_factor * 48 / 64; // Slight dampening for low pawn counts
  }

  return es;
}

} // namespace Eval
} // namespace Prometheus
