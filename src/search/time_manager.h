#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include "search.h"
#include <chrono>

namespace Prometheus {
namespace Search {

class TimeManager {
public:
  void init(const SearchLimits &limits, Color sideToMove);
  bool should_stop(uint64_t accumulated_nodes);
  long long elapsed() const;

  // Config
  int move_overhead = 10;           // ms
  double fail_low_extension = 0.25; // +25% time on fail low
  double panic_factor = 1.35;       // +35% time on panic (late move change)

  void extend_time(double factor);
  void finish_early();

  void update_best_move(Move m, int depth);
  void fail_low();

private:
  std::chrono::high_resolution_clock::time_point start_time;
  int soft_limit; // Target time
  int hard_limit; // Absolute max
  double extension_factor = 1.0;
  bool infinite;
  int nodes_since_check;

  Move last_best_move = Move::NONE;
  int stability_counter = 0;

  int original_soft_limit;
};

extern TimeManager Timer;

} // namespace Search
} // namespace Prometheus

#endif
