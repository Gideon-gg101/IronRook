#ifndef PERFT_H
#define PERFT_H

#include "../board/board.h"

namespace Prometheus {

uint64_t perft(Board &board, int depth);
void perft_divide(Board &board, int depth);

} // namespace Prometheus

#endif // PERFT_H
