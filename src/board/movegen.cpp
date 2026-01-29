#include "movegen.h"
#include "../core/magic.h"
#include <iostream>

namespace Prometheus {
namespace MoveGen {

void generate_all(const Board &board, MoveList &list) {
  Color stm = board.side_to_move();
  Color them = ~stm;
  Bitboard our_pieces = board.pieces(stm);
  Bitboard their_pieces = board.pieces(them);
  Bitboard all_pieces = board.all_pieces();

  // ----------------------------------------------------------------------
  // PAWNS
  // ----------------------------------------------------------------------
  Bitboard pawns = board.pieces(PAWN, stm);
  Bitboard empty = ~all_pieces;
  Direction up = (stm == WHITE) ? NORTH : SOUTH;
  Rank promo_rank = (stm == WHITE) ? RANK_8 : RANK_1;
  int up_offset = (stm == WHITE) ? 8 : -8;

  // Single Push
  Bitboard single_pushes = 0;
  if (stm == WHITE)
    single_pushes = (pawns << 8) & empty;
  else
    single_pushes = (pawns >> 8) & empty;

  Bitboard double_pushes = 0;
  // Double Push: (SinglePush & Rank3/6) shift up & empty
  if (stm == WHITE) {
    Bitboard on_rank3 = single_pushes & (0xFF0000ULL);
    double_pushes = (on_rank3 << 8) & empty;
  } else {
    Bitboard r6 = (single_pushes & 0xFF0000000000ULL);
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
  Bitboard enemies = board.pieces(~stm);

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
  if (stm == WHITE) {
    // NorthWest (+7)
    Bitboard attacks_w = (pawns & 0xFEFEFEFEFEFEFEFEULL) << 7;
    attacks_w &= enemies;
    add_captures(attacks_w, 7);

    // NorthEast (+9)
    Bitboard attacks_e = (pawns & 0x7F7F7F7F7F7F7F7FULL) << 9;
    attacks_e &= enemies;
    add_captures(attacks_e, 9);
  } else {
    // Black Captures (South)
    // SouthEast (-7)
    Bitboard se = (pawns & 0x7F7F7F7F7F7F7F7FULL) >> 7;
    se &= enemies;
    add_captures(se, -7);

    // SouthWest (-9)
    Bitboard sw = (pawns & 0xFEFEFEFEFEFEFEFEULL) >> 9;
    sw &= enemies;
    add_captures(sw, -9);
  }

  // En Passant
  if (board.en_passant_square() != SQ_NONE) {
    Square ep_sq = board.en_passant_square();

    if (stm == WHITE) {
      Square from1 = Square(ep_sq - 7);
      if (Bitboards::get_bit(pawns, from1)) {
        if (File(from1 % 8) != FILE_A && File(from1 % 8) != FILE_H) {
        }
        if (((1ULL << from1) & 0xFEFEFEFEFEFEFEFEULL) && // Not File A
            ((1ULL << from1) << 7) == (1ULL << ep_sq)) {
          list.add(Move(from1, ep_sq, EP_CAPTURE));
        }
      }

      Square from2 = Square(ep_sq - 9);
      if (Bitboards::get_bit(pawns, from2)) {
        if (((1ULL << from2) & 0x7F7F7F7F7F7F7F7FULL) &&
            ((1ULL << from2) << 9) == (1ULL << ep_sq)) {
          list.add(Move(from2, ep_sq, EP_CAPTURE));
        }
      }

    } else {
      Square from1 = Square(ep_sq + 7);
      if (Bitboards::get_bit(pawns, from1)) {
        if (((1ULL << from1) & 0x7F7F7F7F7F7F7F7FULL) &&
            ((1ULL << from1) >> 7) == (1ULL << ep_sq)) {
          list.add(Move(from1, ep_sq, EP_CAPTURE));
        }
      }

      Square from2 = Square(ep_sq + 9);
      if (Bitboards::get_bit(pawns, from2)) {
        if (((1ULL << from2) & 0xFEFEFEFEFEFEFEFEULL) &&
            ((1ULL << from2) >> 9) == (1ULL << ep_sq)) {
          list.add(Move(from2, ep_sq, EP_CAPTURE));
        }
      }
    }
  }

  // Leapers & Sliders
  for (int pt = KNIGHT; pt <= KING; ++pt) {
    Bitboard pieces = board.pieces(PieceType(pt), stm);
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
  if (rights && !board.is_square_attacked(
                    Bitboards::lsb(board.pieces(KING, stm)), them)) {
    Square k_sq = Bitboards::lsb(board.pieces(KING, stm));

    // Indices: 0=WK, 1=WQ, 2=BK, 3=BQ
    int k_idx = (stm == WHITE) ? 0 : 2;
    int q_idx = (stm == WHITE) ? 1 : 3;

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
          board.piece_on(r_sq) == (stm == WHITE ? W_ROOK : B_ROOK)) {
        Square k_dst = (stm == WHITE) ? SQ_G1 : SQ_G8;
        Square r_dst = (stm == WHITE) ? SQ_F1 : SQ_F8;

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
          board.piece_on(r_sq) == (stm == WHITE ? W_ROOK : B_ROOK)) {
        Square k_dst = (stm == WHITE) ? SQ_C1 : SQ_C8;
        Square r_dst = (stm == WHITE) ? SQ_D1 : SQ_D8;

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
