#ifndef SEARCH_H
#define SEARCH_H

#include "../board/board.h"
#include "../board/book.h"
#include "../board/movegen.h"
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace Prometheus {

namespace Search {

struct SearchLimits {
  int depth = 0;
  int nodes = 0;
  int move_time = 0;
  bool infinite = false;

  int white_time = 0;
  int black_time = 0;
  int white_inc = 0;
  int black_inc = 0;
  int moves_to_go = 0;
  int multipv = 1;
  int contempt = 0; // Centipawns (positive = avoid draw)
};

struct RootMove {
  Move move = Move::NONE;
  int score = -2000000;
};

class SearchWorker {
public:
  SearchWorker(int id);
  ~SearchWorker();

  void search(); // Entry point for thread
  void stop();

  // Context from main
  Board rootBoard;
  SearchLimits limits;

  // Control
  int thread_id;
  std::atomic<bool> searching;
  std::atomic<bool> should_stop;

  // Output
  Move bestMove = Move::NONE;

  // Search functions
  void iter_deep();
  int alpha_beta_root(Board &board, int depth, int alpha, int beta,
                      uint64_t &nodes, const std::vector<Move> &excluded);
  int alpha_beta(Board &board, int depth, int ply, int alpha, int beta,
                 uint64_t &nodes, Move excludedMove = Move::NONE,
                 Move prevMove = Move::NONE);
  int quiescence(Board &board, int alpha, int beta, uint64_t &nodes,
                 int depth = 0, int ply = 0);

  // Stats
  uint64_t nodes_searched;
  int static_evals[256]; // For Position Trend (MAX_PLY constraint)
  int root_stability_counter = 0;
  Move last_best_move = Move::NONE;

  // History Heuristic: [Side][From][To]
  // Values -2000 to 2000 approx.
  int history[2][64][64];
  Move killers[128][2];

  // Correction History: [Side][Piece][Square]
  // Adjusts evaluation based on failures/successes of static eval vs search
  // Correction History: [Side][Piece][Square]
  // Adjusts evaluation based on failures/successes of static eval vs search
  int correction_history[2][16][64];

  // Pre-allocated MoveLists to avoid stack overflow
  std::vector<MoveList> moveLists;

  // Continuation History: [PrevPt][PrevTo][CurrPt][CurrTo]
  // 1-ply already exists. Plan mentions "Multi-ply".
  // Let's add CounterMove history: [Side][PrevMove.to][CurrPt][CurrTo] ??
  // Or just CounterMove: [Side][PrevMove.to] -> Move
  // The plan "Counter-Move Heuristic" usually refers to storing the *best
  // response* to a move. Table: counter_moves[Side][PrevMove.to] = Move; (Or
  // full Move struct)
  Move counter_moves[2][64];

  // Continuation History: [PrevPt][PrevTo][CurrPt][CurrTo]
  int continuation_history[16][64][16][64];
  // 2-ply continuation history (response to response) ?
  // continuation_history_2[PrevPrevPt][...]
  // For now, let's stick to 1-ply + CounterMoves to solve Phase B part 2.

  // Root moves for reordering across depths
  std::vector<RootMove> rootMoves;

  void clear_history();
  void update_history(Move m, int bonus, int side);
  void update_correction_history(int side, int depth, int score,
                                 int static_eval, const Board &board);
  int get_correction(int side, const Board &board);

  void update_continuation_history(Move prevMove, Piece prevPiece,
                                   Move currMove, Piece currPiece, int bonus);
  int get_continuation_bonus(Move prevMove, Piece prevPiece, Move currMove,
                             Piece currPiece);

  void score_moves(const Board &board, MoveList &list, Move ttMove, int depth,
                   Move prevMove = Move::NONE);

  // Threading sync
  std::thread thread;
  std::mutex mutex;
  std::condition_variable cv;
  bool exit_thread = false;
  bool start_flag = false;
};

class ThreadPool {
public:
  void init(int count);
  void start_search(const Board &board, const SearchLimits &limits);
  void stop();
  void clear();
  void set_thread_count(int count);

  SearchWorker *main() { return workers.empty() ? nullptr : workers[0]; }

  int default_multipv = 1;
  int default_contempt = 0;
  std::string exp_path; // Added exp_path
  bool use_book = false;
  bool chess960 = false;
  std::string book_path;

  std::vector<SearchWorker *> workers;
};

extern ThreadPool Threads;
extern Book BookInstance;

// Legacy wrapper (optional, or remove)
// void iter_deep(Board &board, SearchLimits limits);

} // namespace Search

} // namespace Prometheus

#endif // SEARCH_H
