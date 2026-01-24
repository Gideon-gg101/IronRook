#ifndef TYPES_H
#define TYPES_H

#include <cassert>
#include <cstdint>
#include <string>

namespace Prometheus {

using Bitboard = uint64_t;

enum Color : int { WHITE = 0, BLACK = 1, COLOR_NB = 2 };

enum PieceType : int {
  NO_PIECE_TYPE = 0,
  PAWN = 1,
  KNIGHT = 2,
  BISHOP = 3,
  ROOK = 4,
  QUEEN = 5,
  KING = 6,
  PIECE_TYPE_NB = 8 // 0..7, where 7 is potentially usable or just bound
};

enum Piece : int {
  NO_PIECE = 0,
  W_PAWN = 1,
  W_KNIGHT,
  W_BISHOP,
  W_ROOK,
  W_QUEEN,
  W_KING,
  B_PAWN = 9,
  B_KNIGHT,
  B_BISHOP,
  B_ROOK,
  B_QUEEN,
  B_KING,
  PIECE_NB = 16
};

enum Square : int {
  SQ_A1,
  SQ_B1,
  SQ_C1,
  SQ_D1,
  SQ_E1,
  SQ_F1,
  SQ_G1,
  SQ_H1,
  SQ_A2,
  SQ_B2,
  SQ_C2,
  SQ_D2,
  SQ_E2,
  SQ_F2,
  SQ_G2,
  SQ_H2,
  SQ_A3,
  SQ_B3,
  SQ_C3,
  SQ_D3,
  SQ_E3,
  SQ_F3,
  SQ_G3,
  SQ_H3,
  SQ_A4,
  SQ_B4,
  SQ_C4,
  SQ_D4,
  SQ_E4,
  SQ_F4,
  SQ_G4,
  SQ_H4,
  SQ_A5,
  SQ_B5,
  SQ_C5,
  SQ_D5,
  SQ_E5,
  SQ_F5,
  SQ_G5,
  SQ_H5,
  SQ_A6,
  SQ_B6,
  SQ_C6,
  SQ_D6,
  SQ_E6,
  SQ_F6,
  SQ_G6,
  SQ_H6,
  SQ_A7,
  SQ_B7,
  SQ_C7,
  SQ_D7,
  SQ_E7,
  SQ_F7,
  SQ_G7,
  SQ_H7,
  SQ_A8,
  SQ_B8,
  SQ_C8,
  SQ_D8,
  SQ_E8,
  SQ_F8,
  SQ_G8,
  SQ_H8,
  SQ_NONE,
  SQUARE_NB = 64
};

enum Direction : int {
  NORTH = 8,
  EAST = 1,
  SOUTH = -8,
  WEST = -1,
  NORTH_EAST = 9,
  SOUTH_EAST = -7,
  SOUTH_WEST = -9,
  NORTH_WEST = 7
};

enum File : int {
  FILE_A,
  FILE_B,
  FILE_C,
  FILE_D,
  FILE_E,
  FILE_F,
  FILE_G,
  FILE_H,
  FILE_NB
};

enum Rank : int {
  RANK_1,
  RANK_2,
  RANK_3,
  RANK_4,
  RANK_5,
  RANK_6,
  RANK_7,
  RANK_8,
  RANK_NB
};

constexpr Square make_square(File f, Rank r) { return Square((r << 3) + f); }

constexpr Bitboard C64(uint64_t v) { return v; }

constexpr int MAX_PLY = 128;

inline Color operator~(Color c) { return Color(c ^ 1); }

// Move: 16 bits
// 0-5: To Square
// 6-11: From Square
// 12-15: Flags
enum MoveFlags {
  QUIET = 0,
  DOUBLE_PAWN_PUSH = 1,
  KING_CASTLE = 2,
  QUEEN_CASTLE = 3,
  CAPTURE = 4,
  EP_CAPTURE = 5,
  // ... specialized promotions
  PROMOTION_KNIGHT = 8,
  PROMOTION_BISHOP = 9,
  PROMOTION_ROOK = 10,
  PROMOTION_QUEEN = 11,
  PROMOTION_KNIGHT_CAPTURE = 12,
  PROMOTION_BISHOP_CAPTURE = 13,
  PROMOTION_ROOK_CAPTURE = 14,
  PROMOTION_QUEEN_CAPTURE = 15
};

struct Move {
  uint16_t data;

  constexpr Move() : data(0) {}
  constexpr Move(uint16_t d) : data(d) {}
  constexpr Move(Square from, Square to, int flags = 0) {
    data = (to & 0x3F) | ((from & 0x3F) << 6) | ((flags & 0xF) << 12);
  }

  constexpr Square to() const { return Square(data & 0x3F); }
  constexpr Square from() const { return Square((data >> 6) & 0x3F); }
  constexpr int flags() const { return (data >> 12) & 0xF; }

  constexpr PieceType promotion_type() const {
    int f = flags();
    if (f & 8) {
      switch (f & 3) {
      case 0:
        return KNIGHT;
      case 1:
        return BISHOP;
      case 2:
        return ROOK;
      case 3:
        return QUEEN;
      }
    }
    return NO_PIECE_TYPE;
  }

  constexpr bool is_capture() const {
    return data & (1 << 14);
  } // Hacky check bit 4 of flags? No. 4=100. capture=4.
  // Is capture if flag & 4?
  // QUIET=0, DOUBLE=1, KC=2, QC=3, CAP=4(0100), EP=5(0101).
  // PROMOTIONS: 8..11 (quiet?), 12..15 (caps).
  // Let's use clean helpers later.

  bool operator==(const Move &m) const { return data == m.data; }
  bool operator!=(const Move &m) const { return data != m.data; }

  std::string to_uci(bool chess960 = false) const {
    if (data == 0)
      return "0000";

    Square f = from();
    Square t = to();

    // Standard Chess Castling Output conversion
    // Internal representation is always King->Rook (Chess960 style)
    if (!chess960) {
      int fgs = flags();
      if (fgs == KING_CASTLE) {
        if (f == SQ_E1)
          return "e1g1";
        if (f == SQ_E8)
          return "e8g8";
      } else if (fgs == QUEEN_CASTLE) {
        if (f == SQ_E1)
          return "e1c1";
        if (f == SQ_E8)
          return "e8c8";
      }
    }

    std::string s = "";
    s += char('a' + (f % 8));
    s += char('1' + (f / 8));
    s += char('a' + (t % 8));
    s += char('1' + (t / 8));

    int fgs = flags();
    // Check for promotion (bit 3 set: 8-15)
    if (fgs & 8) {
      char p = ' ';
      switch (fgs & 3) {
      case 0:
        p = 'n';
        break;
      case 1:
        p = 'b';
        break;
      case 2:
        p = 'r';
        break;
      case 3:
        p = 'q';
        break;
      }
      s += p;
    }
    return s;
  }

  static const Move NONE;
};

inline const Move Move::NONE(0);

} // namespace Prometheus

#endif // TYPES_H
