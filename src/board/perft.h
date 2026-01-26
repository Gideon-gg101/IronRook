#ifndef PERFT_H
#define PERFT_H

#include "../board/board.h"

namespace IroonRook {

uint64_t perft(Board &board, int depth);
void perft_divide(Board &board, int depth);

} // namespace IroonRook

#endif // PERFT_H
