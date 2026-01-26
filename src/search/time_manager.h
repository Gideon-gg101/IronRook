#ifndef TIME_MANAGER_H
#define TIME_MANAGER_H

#include "search.h"
#include <chrono>

namespace IroonRook {
namespace Search {

class TimeManager {
public:
  void init(const SearchLimits &limits, Color sideToMove);
  bool should_stop(uint64_t accumulated_nodes);

  // Config
  int move_overhead = 10; // ms

  void extend_time(double factor);
  void finish_early();

private:
  std::chrono::high_resolution_clock::time_point start_time;
  int soft_limit; // Target time
  int hard_limit; // Absolute max
  double extension_factor = 1.0;
  bool infinite;
  int nodes_since_check;

  int original_soft_limit;
};

extern TimeManager Timer;

} // namespace Search
} // namespace IroonRook

#endif
