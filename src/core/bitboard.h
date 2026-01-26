#ifndef BITBOARD_H
#define BITBOARD_H

#include "types.h"
#include <bit> // C++20 for popcount, countr_zero
#include <iostream>
#include <string>

namespace IroonRook {

namespace Bitboards {

// Print bitboard for debugging
void print(Bitboard bb);

// Operations
constexpr void set_bit(Bitboard &bb, Square s) { bb |= (1ULL << s); }

constexpr void pop_bit(Bitboard &bb, Square s) { bb &= ~(1ULL << s); }

constexpr bool get_bit(Bitboard bb, Square s) { return bb & (1ULL << s); }

constexpr Bitboard square_bb(Square s) { return 1ULL << s; }

// Number of set bits
constexpr int popcount(Bitboard bb) { return std::popcount(bb); }

// Least Significant Bit (index of first set bit)
constexpr Square lsb(Bitboard bb) {
  assert(bb != 0);
  return Square(std::countr_zero(bb));
}

// Reset LSB and return its index
constexpr Square pop_lsb(Bitboard &bb) {
  Square s = lsb(bb);
  bb &= bb - 1; // Kernighan's algorithm
  return s;
}

} // namespace Bitboards

} // namespace IroonRook

#endif // BITBOARD_H
