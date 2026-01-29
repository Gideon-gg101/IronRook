#ifndef MOVEGEN_H
#define MOVEGEN_H

#include "../board/board.h"
#include <vector>

namespace Prometheus {

struct MoveList {
  Move moves[192];
  int scores[192];
  int count;

  MoveList() : count(0) {}

  void add(Move m, int score = 0) {
    if (count < 192) {
      moves[count] = m;
      scores[count] = score;
      count++;
    }
  }
};

namespace MoveGen {

// Generate pseudo-legal moves
void generate_all(const Board &board, MoveList &list);

// Generate legal moves
void generate_legal(Board &board, MoveList &list);

} // namespace MoveGen

} // namespace Prometheus

#endif // MOVEGEN_H
