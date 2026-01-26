#include "uci.h"
#include "../bench.h"
#include "../board/book.h"
#include "../board/perft.h"
#include "../eval/eval.h"
#include "../search/learning.h"
#include "../search/search.h"
#include "../tb/syzygy.h"
#include "../tuning/tuner.h"
#include "../tuning/tuning.h"
#include <iostream>
#include <sstream>
#include <vector>

namespace IroonRook {

namespace UCI {

void loop() {
  Board board;
  // Default start pos
  board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

  // Register tunable parameters
  Tuning::GlobalTuner.register_params();

  // Initialize LMR reduction table
  Search::init_lmr_table();

  std::string line, token;
  while (std::getline(std::cin, line)) {
    if (line.empty())
      continue;
    std::istringstream ss(line);
    ss >> token;

    // ...
    if (token == "uci") {
      std::cout << "id name IroonRook v1.0" << std::endl;
      std::cout << "id author Antigravity" << std::endl;
      std::cout << "option name SyzygyPath type string default <empty>"
                << std::endl;
      std::cout << "option name Threads type spin default 1 min 1 max 128"
                << std::endl;
      std::cout << "option name MultiPV type spin default 1 min 1 max 500"
                << std::endl;
      std::cout << "option name Contempt type spin default 0 min -100 max 100"
                << std::endl;
      std::cout << "option name OwnBook type check default false" << std::endl;
      std::cout << "option name BookPath type string default <empty>"
                << std::endl;
      std::cout << "option name UCI_Chess960 type check default false"
                << std::endl;
      std::cout << "option name ExperiencePath type string default <empty>"
                << std::endl;

      // Print tunable parameters
      Tuning::GlobalTuner.print_params();

      std::cout << "uciok" << std::endl;

      // Init default threads
      Search::Threads.set_thread_count(1);

    } else if (token == "isready") {
      std::cout << "readyok" << std::endl;
    } else if (token == "setoption") {
      std::string name, val;
      ss >> name; // "name"
      if (name == "name") {
        ss >> name;
        if (name == "SyzygyPath") {
          ss >> val;
          if (val == "value") {
            std::string path;
            std::getline(ss, path);
            size_t first = path.find_first_not_of(' ');
            if (first != std::string::npos)
              path = path.substr(first);
            // Syzygy::init(path); // Disabled - missing tbprobe.h dependency
          }
        } else if (name == "Threads") {
          ss >> val;
          if (val == "value") {
            int count;
            ss >> count;
            Search::Threads.set_thread_count(count);
          }
        } else if (name == "MultiPV") {
          ss >> val;
          if (val == "value") {
            // We need to store global multipv or pass it to search?
            // Since 'go' command doesn't usually take multipv as arg in
            // standard UCI (it's an option), we need a global setting or member
            // in UCI/Search namespaces. For now let's store it in a static
            // variable in UCI or Search.
            int mpv;
            ss >> mpv;
            Search::Threads.default_multipv = mpv;
          }
        } else if (name == "Contempt") {
          ss >> val;
          if (val == "value") {
            int cp;
            ss >> cp;
            Search::Threads.default_contempt = cp;
          }
        } else if (name == "OwnBook") {
          ss >> val;
          if (val == "value") {
            std::string v;
            ss >> v;
            Search::Threads.use_book = (v == "true");
          }
        } else if (name == "BookPath") {
          ss >> val;
          if (val == "value") {
            std::string path;
            std::getline(ss, path);
            size_t first = path.find_first_not_of(' ');
            if (first != std::string::npos)
              path = path.substr(first);
            Search::Threads.book_path = path;
            Search::Threads.book_path = path;
            Search::BookInstance.load(path);
          }
        } else if (name == "UCI_Chess960") {
          ss >> val;
          if (val == "value") {
            std::string v;
            ss >> v;
            Search::Threads.chess960 = (v == "true");
          }
        } else if (name == "ExperiencePath") {
          ss >> val;
          if (val == "value") {
            std::string path;
            std::getline(ss, path);
            size_t first = path.find_first_not_of(' ');
            if (first != std::string::npos)
              path = path.substr(first);
            Search::Threads.exp_path = path;
            Search::GlobalExperience.load(path);
          }
        } else {
          // Check for tunable evaluation parameters
          ss >> val; // This should be "value"
          int int_val;
          if (ss >> int_val) {
            for (auto &p : Tuning::GlobalTuner.get_params_list()) {
              if (p.name == name) {
                *p.value_ptr = int_val;
                break;
              }
            }
          }
        }
      }
    } else if (token == "ucinewgame") {
      Search::Threads.clear();
      board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    } else if (token == "position") {
      position(line, board);
    } else if (token == "go") {
      go(line, board);
    } else if (token == "quit") {
      if (!Search::Threads.exp_path.empty()) {
        Search::GlobalExperience.save(Search::Threads.exp_path);
      }
      Search::Threads.stop();
      break;
    } else if (token == "stop") {
      Search::Threads.stop();
    } else if (token == "d") {
      std::cout << board.to_fen() << std::endl;
    } else if (token == "bench") {
      // Default bench depth 12 if not specified?
      // check if next token is depth?
      int depth = 12;
      // Simple parsing: if we see "depth X" ... but token is already consumed.
      // Bench command is usually just "bench" or "bench <depth>"
      // Let's check remaining line
      if (ss >> depth) {
        // depth read successful
      }
      Benchmark::bench(depth);
    } else if (token == "perft") {
      int depth = 1;
      if (ss >> depth) {
        perft_divide(board, depth);
      } else {
        std::cout << "Usage: perft <depth>" << std::endl;
      }
    } else if (token == "expdecay") {
      Search::GlobalExperience.decay();
      std::cout << "info string Experience cache decayed." << std::endl;
    } else if (token == "exportparams") {
      std::string filename;
      if (ss >> filename) {
        Tuning::GlobalTuner.export_params(filename);
      } else {
        std::cout << "Usage: exportparams <filename>" << std::endl;
      }
    } else if (token == "importparams") {
      std::string filename;
      if (ss >> filename) {
        Tuning::GlobalTuner.import_params(filename);
      } else {
        std::cout << "Usage: importparams <filename>" << std::endl;
      }
    } else if (token == "tune") {
      std::string filename;
      int iter = 1000;
      if (ss >> filename) {
        if (!(ss >> iter))
          iter = 1000;
        Tuning::Tuner::init();
        Tuning::Tuner::load_file(filename);
        Tuning::Tuner::run(iter);
      } else {
        std::cout << "Usage: tune <epd_file> [iterations]" << std::endl;
      }
    } else if (token == "trace_eval" || token == "eval") {
      // "eval" or "trace_eval": print static evaluation details
      // Use trace if cmd is trace_eval or just "eval" for now (useful debug)
      Eval::EvalTrace trace;
      int score = Eval::evaluate_trace(board, trace);

      std::cout << "Evaluation Trace for FEN: " << board.to_fen() << std::endl;
      std::cout << "Total Score: " << score << " cp" << std::endl;
      std::cout << "--- Features ---" << std::endl;
      for (auto &term : trace.terms) {
        std::cout << term.first << ": " << term.second << std::endl;
      }
      std::cout << "--- End Trace ---" << std::endl;
    }
  }
}

// ...

void go(const std::string &command, Board &board) {
  // Parse time/depth
  Search::SearchLimits limits;

  std::stringstream ss(command);
  std::string token;
  while (ss >> token) {
    if (token == "depth")
      ss >> limits.depth;
    else if (token == "nodes")
      ss >> limits.nodes;
    else if (token == "wtime")
      ss >> limits.white_time;
    else if (token == "btime")
      ss >> limits.black_time;
    else if (token == "winc")
      ss >> limits.white_inc;
    else if (token == "binc")
      ss >> limits.black_inc;
    else if (token == "movestogo")
      ss >> limits.moves_to_go;
    else if (token == "movetime")
      ss >> limits.move_time;
    else if (token == "infinite")
      limits.infinite = true;
  }

  // Non-blocking start
  Search::Threads.start_search(board, limits);
}

void position(const std::string &command, Board &board) {
  // Format: position [startpos | fen <fenstring>] [moves <moves>]
  size_t pos = command.find("moves");
  std::string fen_part = command.substr(0, pos);

  if (fen_part.find("startpos") != std::string::npos) {
    board.set_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
  } else if (fen_part.find("fen") != std::string::npos) {
    size_t fen_start = fen_part.find("fen") + 4;
    std::string fen = fen_part.substr(fen_start);
    board.set_fen(fen);
  }

  if (pos != std::string::npos) {
    std::string moves_str = command.substr(pos + 6);
    std::istringstream ss(moves_str);
    std::string move_token;
    while (ss >> move_token) {
      // Parse move string (e.g. e2e4)
      MoveList list;
      MoveGen::generate_legal(board, list);

      // Linear scan for matching move (inefficient but safe for MVP)
      for (int i = 0; i < list.count; ++i) {
        Move m = list.moves[i];
        Square f = m.from();
        Square t = m.to();
        // Convert sq to string
        std::string m_str = "";
        m_str += (char)('a' + (f % 8));
        m_str += (char)('1' + (f / 8));
        m_str += (char)('a' + (t % 8));
        m_str += (char)('1' + (t / 8));

        // Check promotion char
        if (m.promotion_type() != NO_PIECE_TYPE) {
          char p_char = ' ';
          PieceType pt = m.promotion_type();
          if (pt == KNIGHT)
            p_char = 'n';
          else if (pt == BISHOP)
            p_char = 'b';
          else if (pt == ROOK)
            p_char = 'r';
          else if (pt == QUEEN)
            p_char = 'q';
          m_str += p_char;
        }

        bool match = (m_str == move_token);

        // Standard Chess Castling Compatibility (e1c1 vs e1a1)
        if (!match && (m.flags() == KING_CASTLE || m.flags() == QUEEN_CASTLE) &&
            !Search::Threads.chess960) {
          std::string std_str = "";
          Square f = m.from();
          std_str += (char)('a' + (f % 8));
          std_str += (char)('1' + (f / 8));

          Square t = SQ_NONE;
          if (board.side_to_move() == WHITE) {
            t = (m.flags() == KING_CASTLE) ? SQ_G1 : SQ_C1;
          } else {
            t = (m.flags() == KING_CASTLE) ? SQ_G8 : SQ_C8;
          }
          std_str += (char)('a' + (t % 8));
          std_str += (char)('1' + (t / 8));

          if (std_str == move_token)
            match = true;
        }

        if (match) {
          board.make_move(m);
          break;
        }
      }
      // Check if move was applied?
      // No easy way check 'i' outside loop here efficiently without flag
      // But let's assume if it fails we don't apply.
    }
  }
}

} // namespace UCI

} // namespace IroonRook
