#include "perft.h"
#include "movegen.h"
#include <chrono>
#include <iostream>

namespace IroonRook {

uint64_t perft(Board &board, int depth) {
  if (depth == 0)
    return 1ULL;

  MoveList list;
  MoveGen::generate_legal(board, list);

  if (depth == 1)
    return list.count;

  uint64_t nodes = 0;
  for (int i = 0; i < list.count; ++i) {
    Board copy = board;
    if (copy.make_move(list.moves[i])) {
      nodes += perft(copy, depth - 1);
    }
  }
  return nodes;
}

void perft_divide(Board &board, int depth) {
  auto start = std::chrono::high_resolution_clock::now();

  MoveList list;
  MoveGen::generate_legal(board, list);

  uint64_t total_nodes = 0;
  for (int i = 0; i < list.count; ++i) {
    Move m = list.moves[i];
    Board copy = board;
    if (copy.make_move(m)) {
      uint64_t nodes = perft(copy, depth - 1);
      std::cout << m.from() << m.to() << ": " << nodes << "\n";
      total_nodes += nodes;
    }
  }

  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> elapsed = end - start;

  std::cout << "\nNodes searched: " << total_nodes << "\n";
  std::cout << "Time: " << elapsed.count() * 1000 << " ms\n";
  std::cout << "NPS: " << (uint64_t)(total_nodes / elapsed.count()) << "\n";
}

} // namespace IroonRook
