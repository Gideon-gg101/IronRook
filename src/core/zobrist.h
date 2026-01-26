#ifndef ZOBRIST_H
#define ZOBRIST_H

#include "types.h"

namespace IroonRook {

namespace Zobrist {

extern uint64_t piece_keys[PIECE_NB][SQUARE_NB];
extern uint64_t en_passant_keys[FILE_NB];
extern uint64_t castle_keys[16];
extern uint64_t side_key;

void init();

} // namespace Zobrist

} // namespace IroonRook

#endif // ZOBRIST_H
