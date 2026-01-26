#include "bench.h"
#include "board/board.h"
#include "board/movegen.h"
#include "board/perft.h"
#include "core/bitboard.h"
#include "core/magic.h"
#include "core/zobrist.h"
#include "interface/selfplay.h" // Include header
#include "interface/uci.h"
#include "search/search.h"
#include <iostream>

using namespace IroonRook;

int main(int argc, char *argv[]) {
  Magic::init();
  Zobrist::init();

  if (argc > 1 && std::string(argv[1]) == "bench") {
    Benchmark::bench(13);
  } else if (argc > 1 && std::string(argv[1]) == "selfplay") {
    // selfplay <games> <depth> [output_file]
    int g = 10;
    int d = 6;
    std::string outfile = "";
    if (argc > 2)
      g = std::stoi(argv[2]);
    if (argc > 3)
      d = std::stoi(argv[3]);
    if (argc > 4)
      outfile = std::string(argv[4]);

    IroonRook::run_selfplay(g, d, outfile);
  } else if (argc > 1 && std::string(argv[1]) == "perft") {
    int d = 5;
    if (argc > 2)
      d = std::stoi(argv[2]);
    Board board;
    board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    IroonRook::perft_divide(board, d);
  } else {
    // Init threads before UCI loop
    Search::Threads.init(1);
    UCI::loop();
  }

  return 0;
}
