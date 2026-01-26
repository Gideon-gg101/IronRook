#include "bench.h"
#include "board/board.h"
#include "core/tt.h"
#include "search/search.h"
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

namespace IroonRook {

namespace Benchmark {

// Standard positions for benchmarking
const std::vector<std::string> fens = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8",
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10",
    "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1",
    "r1b1k2r/pp1p1p1p/1qn1p1p1/2b5/2P1P1n1/1PNB1N2/P1Q2PPP/R1B1K2R w KQkq - 1 "
    "10",
    "r1b1k2r/pp1p1p1p/1qn1p1p1/2b5/2P1P1n1/2PNB1N1/P1Q2PPP/R1B1K2R w KQkq - 1 "
    "10"};

void bench(int depth) {
  uint64_t totalNodes = 0;
  auto start = std::chrono::high_resolution_clock::now();

  Search::SearchLimits limits;
  limits.depth = depth;

  if (Search::Threads.workers.empty()) {
    Search::Threads.set_thread_count(1);
  }

  std::cout << "Benchmarking " << fens.size() << " positions at depth " << depth
            << "..." << std::endl;

  for (const auto &fen : fens) {
    Board board;
    board.set_fen(fen);

    Search::Threads.start_search(board, limits);

    // Wait for search to finish
    while (true) {
      bool running = false;
      for (auto w : Search::Threads.workers) {
        if (w->searching)
          running = true;
      }
      if (!running)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    uint64_t posNodes = 0;
    for (auto w : Search::Threads.workers) {
      posNodes += w->nodes_searched;
    }
    totalNodes += posNodes;

    // Clear TT between bench runs
    TT.clear();
  }

  auto end = std::chrono::high_resolution_clock::now();
  long long duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
          .count();

  std::cout << "===========================" << std::endl;
  std::cout << "Total Nodes: " << totalNodes << std::endl;
  std::cout << "Total Time : " << duration << " ms" << std::endl;
  std::cout << "NPS        : "
            << (duration > 0 ? (totalNodes * 1000 / duration) : 0) << std::endl;
  std::cout << "===========================" << std::endl;
}

} // namespace Benchmark

} // namespace IroonRook
