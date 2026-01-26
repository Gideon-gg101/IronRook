#include "selfplay.h"
#include "../board/board.h"
#include "../board/movegen.h"
#include "../core/tt.h"
#include "../search/search.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>


namespace IroonRook {

void run_selfplay(int games, int depth, const std::string &outputFile) {
  std::cout << "Starting Self-Play: " << games << " games at depth " << depth
            << std::endl;

  std::ofstream outfile;
  bool save_data = !outputFile.empty();
  if (save_data) {
    outfile.open(outputFile, std::ios::app);
    if (!outfile.is_open()) {
      std::cerr << "Error: Could not open output file " << outputFile
                << std::endl;
      save_data = false;
    } else {
      std::cout << "Recording data to " << outputFile << std::endl;
    }
  }

  Search::SearchLimits limits;
  limits.depth = depth;
  limits.move_time = 0;
  limits.nodes = 0;

  int w_wins = 0;
  int b_wins = 0;
  int draws = 0;

  for (int g = 1; g <= games; ++g) {
    Board board;
    board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    int moves = 0;
    int result = 0; // 0=running, 1=white win, 2=black win, 3=draw
    std::vector<std::string> game_fens;

    while (true) {
      if (save_data && moves >= 8) {
        game_fens.push_back(board.to_fen());
      }

      // Allocate SearchWorker on heap to avoid stack overflow (Continuation
      // History is large)
      auto worker = std::make_unique<Search::SearchWorker>(0);
      worker->rootBoard = board;
      worker->limits = limits;
      worker->thread_id = 0;
      worker->should_stop = false;

      worker->iter_deep();

      Move bestMove = worker->bestMove;

      if (bestMove == Move::NONE) {
        std::cout << "Error: No best move found!" << std::endl;
        break;
      }

      board.make_move(bestMove);
      moves++;

      MoveList legal;
      MoveGen::generate_legal(board, legal);

      if (legal.count == 0) {
        // Checkmate detection
        Square k_sq = Bitboards::lsb(board.pieces(KING, board.side_to_move()));
        bool in_check = board.is_square_attacked(k_sq, ~board.side_to_move());

        if (in_check) {
          result = (board.side_to_move() == WHITE) ? 2 : 1;
        } else {
          result = 3;
        }
        break;
      }

      if (board.is_repetition() || board.half_move_clock() >= 100) {
        result = 3;
        break;
      }

      if (moves > 300) {
        result = 3;
        break;
      }
    }
    if (result == 1)
      w_wins++;
    else if (result == 2)
      b_wins++;
    else
      draws++;

    std::cout << "Game " << g << ": "
              << (result == 1 ? "White Win"
                              : (result == 2 ? "Black Win" : "Draw"))
              << " (" << moves << " moves)" << std::endl;

    if (save_data) {
      std::string res_str = "0.5";
      if (result == 1)
        res_str = "1.0";
      else if (result == 2)
        res_str = "0.0";

      for (const auto &fen : game_fens) {
        outfile << fen << " | " << res_str << "\n";
      }
    }
  }

  std::cout << "Result: + " << w_wins << " - " << b_wins << " = " << draws
            << std::endl;
  if (save_data)
    outfile.close();
}

} // namespace IroonRook
