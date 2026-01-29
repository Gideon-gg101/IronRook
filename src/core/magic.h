#ifndef MAGIC_H
#define MAGIC_H

#include "types.h"

namespace Prometheus {

namespace Magic {

void init();

Bitboard get_rook_attacks(Square sq, Bitboard occ);
Bitboard get_bishop_attacks(Square sq, Bitboard occ);
Bitboard get_queen_attacks(Square sq, Bitboard occ);

Bitboard get_knight_attacks(Square sq);
Bitboard get_king_attacks(Square sq);

} // namespace Magic

} // namespace Prometheus

#endif // MAGIC_H
