#include "bitboard.h"
#include <iostream>

namespace IroonRook {
namespace Bitboards {

void print(Bitboard bb) {
  std::cout << "\n";
  for (int rank = RANK_8; rank >= RANK_1; --rank) {
    std::cout << (rank + 1) << "  ";
    for (int file = FILE_A; file <= FILE_H; ++file) {
      Square sq = make_square(File(file), Rank(rank));
      if (get_bit(bb, sq))
        std::cout << "X ";
      else
        std::cout << ". ";
    }
    std::cout << "\n";
  }
  std::cout << "\n   A B C D E F G H\n\n";
  std::cout << "   Bitboard: " << bb << "\n\n";
}

} // namespace Bitboards
} // namespace IroonRook
