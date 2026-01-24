#include "movegen.h"
#include "../core/magic.h"
#include <iostream>

namespace Prometheus {
namespace MoveGen {

void generate_all(const Board &board, MoveList &list) {
  Color us = board.side_to_move();
  Color them = ~us;
  Bitboard our_pieces = board.pieces(us);
  Bitboard their_pieces = board.pieces(them);
  Bitboard all_pieces = board.all_pieces();

  // ----------------------------------------------------------------------
  // PAWNS
  // ----------------------------------------------------------------------
  Bitboard pawns = board.pieces(PAWN, us);
  Bitboard empty = ~all_pieces;
  Direction up = (us == WHITE) ? NORTH : SOUTH;
  Rank promo_rank = (us == WHITE) ? RANK_8 : RANK_1;
  Rank start_rank = (us == WHITE) ? RANK_2 : RANK_7;
  int up_offset = (us == WHITE) ? 8 : -8;

  // Single Push
  // Shift pawns 'up' and AND with empty
  // Implementation: loop or bitwise. Loop is easier for move list population
  // with serialization.
  Bitboard single_pushes = 0;
  if (us == WHITE)
    single_pushes = (pawns << 8) & empty;
  else
    single_pushes = (pawns >> 8) & empty;

  Bitboard double_pushes = 0;
  // Double Push: (SinglePush & Rank3/6) shift up & empty
  if (us == WHITE) {
    Bitboard on_rank3 = single_pushes & (0xFF0000ULL);
    // Rank 3 mask: 0xFF0000 (Bits 16-23)
    // Check if single push landed on rank 3 AND the square above is empty.
    // Logic: (pawns << 8) & empty -> single_pushes (on rank 3).
    // (single_pushes & Rank3) -> redundant check if correct but safe.
    // Then shift << 8 to Rank 4.
    double_pushes = (on_rank3 << 8) & empty;
  } else {
    Bitboard r6 = (single_pushes & 0xFF0000000000ULL);
    // Rank 6 mask: 0xFF0000000000 (Bits 40-47)
    double_pushes = (r6 >> 8) & empty;
  }

  // Serialize Pushes
  // Single
  Bitboard p1 = single_pushes;
  while (p1) {
    Square to = Bitboards::pop_lsb(p1);
    Square from = Square(to - up_offset);

    if (Rank(to / 8) == promo_rank) {
      list.add(Move(from, to, PROMOTION_QUEEN));
      list.add(Move(from, to, PROMOTION_ROOK));
      list.add(Move(from, to, PROMOTION_BISHOP));
      list.add(Move(from, to, PROMOTION_KNIGHT));
    } else {
      list.add(Move(from, to, QUIET));
    }
  }

  // Double
  Bitboard p2 = double_pushes;
  while (p2) {
    Square to = Bitboards::pop_lsb(p2);
    Square from = Square(to - 2 * up_offset);
    list.add(Move(from, to, DOUBLE_PAWN_PUSH));
  }

  // Captures
  Bitboard enemies = board.pieces(~us);
  // ... Implement captures logic ...
  // Left/Right attacks
  // For now, MVP Startpos doesn't capture.
  // I will add them for completeness.

  // Helper for capture generation
  auto add_captures = [&](Bitboard attacks, int diff) {
    while (attacks) {
      Square to = Bitboards::pop_lsb(attacks);
      Square from = Square(to - diff);

      if (Rank(to / 8) == promo_rank) {
        list.add(Move(from, to, PROMOTION_QUEEN_CAPTURE));
        list.add(Move(from, to, PROMOTION_ROOK_CAPTURE));
        list.add(Move(from, to, PROMOTION_BISHOP_CAPTURE));
        list.add(Move(from, to, PROMOTION_KNIGHT_CAPTURE));
      } else {
        list.add(Move(from, to, CAPTURE));
      }
    }
  };

  // White Captures
  if (us == WHITE) {
    // NorthWest (+7)
    // Up(8) Left(-1) = 7. Block File A.
    Bitboard attacks_w = (pawns & 0xFEFEFEFEFEFEFEFEULL) << 7;
    attacks_w &= enemies;
    add_captures(attacks_w, 7);

    // NorthEast (+9)
    // Up(8) Right(+1) = 9. Block File H.
    Bitboard attacks_e = (pawns & 0x7F7F7F7F7F7F7F7FULL) << 9;
    attacks_e &= enemies;
    add_captures(attacks_e, 9);
  } else {
    // Black Captures (South)
    // SouthEast (-7) (from perspective of index? No. -7 is NW for Black? No)
    // Black Pawn on H7 (63). -7 -> A7 (56)? No. 63-7 = 56 (A7). Wrap?
    // H7 is index 63. A7 is 56?
    // Rank 7: A7=56, B7=57 ... H7=63.
    // H7 captures G6 (46). H7(63) -> G6(63-9=54? No. H is 7. G is 6. Row 7->6
    // (-8). Col 7->6 (-1). Total -9.) So -9 is one capture. H7 captures I6? No.
    // A7(56) captures B6(49). 56-49=7.
    // So Black captures are -7 and -9.

    // Capture -7 (SouthEast from Black view? No. A7->B6 is +1 col, -1 row. SE.
    // Index -7.) Mask out File H (can't capture from H to "I")? If on H, -7
    // lands on A of prev rank. WRONG. H can't capture 'Right' (Visual). A->B is
    // 'Right' (Visual). Wait. A is Left. Black structure: A7 . . . . B6 . .
    // Capture A7->B6 is "South East".

    Bitboard se = (pawns & 0x7F7F7F7F7F7F7F7FULL) >> 7;
    se &= enemies;
    add_captures(se, -7);

    Bitboard sw = (pawns & 0xFEFEFEFEFEFEFEFEULL) >> 9;
    sw &= enemies;
    add_captures(sw, -9);
  }
  // En Passant
  if (board.en_passant_square() != SQ_NONE) {
    Square ep_sq = board.en_passant_square();
    // Bitboard ep_bb = Bitboards::square_bb(ep_sq); // Not used if we construct
    // shifts

    if (us == WHITE) {
      // White captures EP on Rank 6 (target ep_sq)
      // Sources on Rank 5: ep_sq - 7 (attack from left-behind?), ep_sq - 9.
      // Directions from Pawn perspective:
      // Pawn at 'from' captures 'to' (ep_sq).
      // from + 7 = to (NorthWest). from = to - 7.
      // from + 9 = to (NorthEast). from = to - 9.

      // Check capture from Right (NorthWest capture: +7)
      // Source must NOT be on File A? (Because +7 wraps).
      // If ep_sq is on File A? No, impossible (pawn must be on B to be captured
      // left?). Logic: valid sources for +7 capture to 'ep_sq'. from = ep_sq
      // - 7. Check if from has White Pawn. Also check if 'from' is logically
      // valid (e.g. adjacent file). File of (ep_sq - 7) vs File of ep_sq.
      // Should be diff 1.

      // Simplest: Check pseudo-attackers TO ep_sq
      // Enemy Pawn Attacks FROM ep_sq give squares that attack ep_sq.
      // But ep_sq is empty.
      // Use existing capture logic masks?
      // capture to ep_sq (NorthWest): source is (ep_sq - 7).
      // Check if (ep_sq - 7) is occupied by us, AND (ep_sq - 7) is NOT File H
      // (wrap risk?). Wait. 'from' -> 'to' (+7). 'from' File must not be A.
      // 'to' (ep_sq) is valid.
      // Mask: (pawns & ~FileA) << 7.
      // If we reverse: to >> 7 -> form.
      // to (ep_sq).
      // Candidate 1: from = ep_sq - 7.
      // Check if (from) has our pawn mid-board.
      // Check File(from) vs File(to) distance.

      Square from1 = Square(ep_sq - 7);
      if (Bitboards::get_bit(pawns, from1)) {
        // Check wrap: File A -> H impossible for +7?
        // File(from1) + 1 == File(to)? No, NW is Left. File(from1) - 1.
        // from(File B) -> to(File A). Index: 9 -> 16 (Wait. 9+7=16). B2->A3.
        // Correct.
        // If from1 is File A... 0 + 7 = 7 (File H same rank).
        // So from1 cannot be File A.
        if (File(from1 % 8) != FILE_A &&
            File(from1 % 8) != FILE_H) { // Safe check
          // Logic: if from1 is B..H, +7 is valid.
          // if from1 is A, +7 wraps.
        }
        // Better:
        if (((1ULL << from1) & 0xFEFEFEFEFEFEFEFEULL) && // Not File A
            ((1ULL << from1) << 7) == (1ULL << ep_sq)) { // Double check bits
          list.add(Move(from1, ep_sq, EP_CAPTURE));
        }
      }

      Square from2 = Square(ep_sq - 9);
      if (Bitboards::get_bit(pawns, from2)) {
        // NE (+9). Block File H.
        if (((1ULL << from2) & 0x7F7F7F7F7F7F7F7FULL) &&
            ((1ULL << from2) << 9) == (1ULL << ep_sq)) {
          list.add(Move(from2, ep_sq, EP_CAPTURE));
        }
      }

    } else {
      // Black Captures EP on Rank 3 (target ep_sq)
      // Sources on Rank 4.
      // Directions: SouthEast (-7), SouthWest (-9).
      // Black moves -7 (SE) or -9 (SW).
      // from - 7 = to. from = to + 7.
      // from - 9 = to. from = to + 9.

      Square from1 = Square(ep_sq + 7);
      if (Bitboards::get_bit(pawns, from1)) {
        // Attack -7 (SE). Exclude File H source (can't go right).
        if (((1ULL << from1) & 0x7F7F7F7F7F7F7F7FULL) &&
            ((1ULL << from1) >> 7) == (1ULL << ep_sq)) {
          list.add(Move(from1, ep_sq, EP_CAPTURE));
        }
      }

      Square from2 = Square(ep_sq + 9);
      if (Bitboards::get_bit(pawns, from2)) {
        // Attack -9 (SW). Exclude File A source (can't go left).
        if (((1ULL << from2) & 0xFEFEFEFEFEFEFEFEULL) &&
            ((1ULL << from2) >> 9) == (1ULL << ep_sq)) {
          list.add(Move(from2, ep_sq, EP_CAPTURE));
        }
      }
    }
  }

  // Leapers & Sliders
  for (int pt = KNIGHT; pt <= KING; ++pt) {
    Bitboard pieces = board.pieces(PieceType(pt), us);
    while (pieces) {
      Square from = Bitboards::pop_lsb(pieces);
      Bitboard attacks = 0;

      if (pt == KNIGHT)
        attacks = Magic::get_knight_attacks(from);
      else if (pt == BISHOP)
        attacks = Magic::get_bishop_attacks(from, all_pieces);
      else if (pt == ROOK)
        attacks = Magic::get_rook_attacks(from, all_pieces);
      else if (pt == QUEEN)
        attacks = Magic::get_queen_attacks(from, all_pieces);
      else if (pt == KING)
        attacks = Magic::get_king_attacks(from);

      // Filter out own pieces
      attacks &= ~our_pieces;

      while (attacks) {
        Square to = Bitboards::pop_lsb(attacks);
        // Flag: Capture or Quiet?
        int flags = 0;
        if (Bitboards::get_bit(their_pieces, to))
          flags = CAPTURE;
        list.add(Move(from, to, flags));
      }
    }
  }

  // ----------------------------------------------------------------------
  // CASTLING (Chess960 Compatible)
  // ----------------------------------------------------------------------
  int rights = board.castling_rights();
  if (rights &&
      !board.is_square_attacked(Bitboards::lsb(board.pieces(KING, us)), them)) {
    Square k_sq = Bitboards::lsb(board.pieces(KING, us));

    // Indices: 0=WK, 1=WQ, 2=BK, 3=BQ
    int k_idx = (us == WHITE) ? 0 : 2;
    int q_idx = (us == WHITE) ? 1 : 3;

    // Helper to check path clear
    auto is_path_clear = [&](Square s1, Square s2, Square exclude) {
      int start = std::min((int)s1, (int)s2);
      int end = std::max((int)s1, (int)s2);
      for (int s = start + 1; s < end; ++s) {
        if (Square(s) == exclude)
          continue;
        if (board.piece_on(Square(s)) != NO_PIECE)
          return false;
      }
      return true;
    };

    // Helper to check path safe (not attacked)
    auto is_path_safe = [&](Square s1, Square s2) {
      int start = std::min((int)s1, (int)s2);
      int end = std::max((int)s1, (int)s2);
      for (int s = start; s <= end; ++s) {
        if (Square(s) == k_sq)
          continue;
        if (board.is_square_attacked(Square(s), them))
          return false;
      }
      return true;
    };

    // King-Side
    if (rights & (1 << k_idx)) {
      Square r_sq = board.castling_rook(k_idx);
      if (r_sq != SQ_NONE &&
          board.piece_on(r_sq) == (us == WHITE ? W_ROOK : B_ROOK)) {
        Square k_dst = (us == WHITE) ? SQ_G1 : SQ_G8;
        Square r_dst = (us == WHITE) ? SQ_F1 : SQ_F8;

        bool path_ok = is_path_clear(k_sq, k_dst, r_sq);
        if (path_ok)
          path_ok = is_path_clear(r_sq, r_dst, k_sq);

        if (path_ok && is_path_safe(k_sq, k_dst)) {
          list.add(Move(k_sq, r_sq, KING_CASTLE));
        }
      }
    }

    // Queen-Side
    if (rights & (1 << q_idx)) {
      Square r_sq = board.castling_rook(q_idx);
      if (r_sq != SQ_NONE &&
          board.piece_on(r_sq) == (us == WHITE ? W_ROOK : B_ROOK)) {
        Square k_dst = (us == WHITE) ? SQ_C1 : SQ_C8;
        Square r_dst = (us == WHITE) ? SQ_D1 : SQ_D8;

        bool path_ok = is_path_clear(k_sq, k_dst, r_sq);
        if (path_ok)
          path_ok = is_path_clear(r_sq, r_dst, k_sq);

        if (path_ok && is_path_safe(k_sq, k_dst)) {
          list.add(Move(k_sq, r_sq, QUEEN_CASTLE));
        }
      }
    }
  }
}

void generate_legal(Board &board, MoveList &list) {
  MoveList pseudo;
  generate_all(board, pseudo);

  for (int i = 0; i < pseudo.count; ++i) {
    Board copy = board;
    if (copy.make_move(pseudo.moves[i])) {
      list.add(pseudo.moves[i]);
    }
  }
}

} // namespace MoveGen
} // namespace Prometheus
