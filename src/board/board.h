#ifndef BOARD_H
#define BOARD_H

#include "../core/bitboard.h"
#include "../core/types.h"
#include <array>
#include <string>
#include <vector>

namespace IroonRook {

class Board {
public:
  Board();

  // Set position from FEN (TODO)
  // Set position from FEN
  void set_fen(const std::string &fen);
  std::string to_fen() const;

  // Get bitboard for a specific piece type and color
  Bitboard pieces(PieceType pt, Color c) const;
  Bitboard pieces(Color c) const;
  Bitboard pieces(PieceType pt) const; // All pieces of type
  Bitboard all_pieces() const;

  // Piece at square
  Piece piece_on(Square s) const;

  // State
  Color side_to_move() const;
  Square en_passant_square() const;
  int castling_rights() const;
  int half_move_clock() const;
  Square castling_rook(int index) const;

  // Check if a square is attacked by side 'by_side'
  bool is_square_attacked(Square sq, Color by_side) const;

  // Modifiers (used by do_move)
  void put_piece(Piece p, Square s);
  void remove_piece(Square s);

  // Make move (minimal for perft)
  bool make_move(Move m); // Returns false if illegal (king left in check)
  void make_null_move();

  // Static Exchange Evaluation
  bool see(Move m, int threshold) const;

  // Zobrist Hash
  uint64_t key;

  // Incremental Evaluation
  int mg_value;
  int eg_value;
  int phase_value;
  std::vector<uint64_t> history;

  bool is_repetition() const;

private:
  std::array<Bitboard, PIECE_TYPE_NB> by_type_bb;
  std::array<Bitboard, COLOR_NB> by_color_bb;
  std::array<Piece, SQUARE_NB> board_array;

  Color side;
  Square ep_square;
  int castle_rights;        // Bitmask: WK=1, WQ=2, BK=4, BQ=8
  Square castling_rooks[4]; // 0=WK, 1=WQ, 2=BK, 3=BQ (Stores source square of
                            // rook)
  int half_moves;
  int full_moves;
};

// Inline implementations
inline Bitboard Board::pieces(PieceType pt, Color c) const {
  return by_type_bb[pt] & by_color_bb[c];
}

inline Bitboard Board::pieces(Color c) const { return by_color_bb[c]; }

inline Bitboard Board::pieces(PieceType pt) const { return by_type_bb[pt]; }

inline Bitboard Board::all_pieces() const {
  return by_color_bb[WHITE] | by_color_bb[BLACK];
}

inline Piece Board::piece_on(Square s) const { return board_array[s]; }

inline Color Board::side_to_move() const { return side; }
inline Square Board::en_passant_square() const { return ep_square; }
inline int Board::castling_rights() const { return castle_rights; }
inline int Board::half_move_clock() const { return half_moves; }
inline Square Board::castling_rook(int index) const {
  return castling_rooks[index];
}

} // namespace IroonRook

#endif // BOARD_H
