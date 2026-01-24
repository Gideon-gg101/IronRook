#include "time_manager.h"
#include <iostream>

namespace Prometheus {
namespace Search {

TimeManager Timer;

void TimeManager::init(const SearchLimits &limits, Color sideToMove) {
  start_time = std::chrono::high_resolution_clock::now();
  nodes_since_check = 0;
  infinite = limits.infinite;
  extension_factor = 1.0;

  if (infinite || limits.depth > 0 || limits.nodes > 0) {
    soft_limit = 2000000000; // Effectively infinite
    hard_limit = 2000000000;
    return;
  }

  // Check if any time control was actually provided
  bool time_control_given =
      (limits.white_time > 0 || limits.black_time > 0 || limits.move_time > 0);

  if (!time_control_given) {
    // No limits at all? Treat as infinite
    infinite = true;
    soft_limit = 2000000000;
    hard_limit = 2000000000;
    return;
  }

  // Default: timed game
  int time_left = (sideToMove == WHITE) ? limits.white_time : limits.black_time;
  int inc = (sideToMove == WHITE) ? limits.white_inc : limits.black_inc;
  int moves_to_go = limits.moves_to_go;

  // Allocation logic: More aggressive use of time in midgame
  double factor = 1.0 / 20.0;
  if (time_left < 60000)
    factor = 1.0 / 40.0; // Conserve time when low

  soft_limit = (int)(time_left * factor) + inc;

  // Safety context
  hard_limit = time_left - 50 - move_overhead;
  if (hard_limit < 10)
    hard_limit = 10;

  if (soft_limit > hard_limit)
    soft_limit = hard_limit;
  if (soft_limit < 10)
    soft_limit = 10;

  original_soft_limit = soft_limit;

  // Special case: Exact time per move
  if (limits.move_time > 0) {
    soft_limit = limits.move_time - move_overhead;
    hard_limit = limits.move_time - move_overhead;
    original_soft_limit = soft_limit;
  }
}

void TimeManager::extend_time(double factor) {
  if (extension_factor < factor) {
    extension_factor = factor;
    // Cap max extension at 2.5x
    if (extension_factor > 2.5)
      extension_factor = 2.5;
  }
}

void TimeManager::finish_early() {
  soft_limit = (int)(soft_limit * 0.75); // Reduce soft limit
}

bool TimeManager::should_stop(uint64_t accumulated_nodes) {
  if (infinite)
    return false;

  // Check every 2048 nodes to reduce overhead
  if ((accumulated_nodes & 2047) != 0)
    return false;

  auto now = std::chrono::high_resolution_clock::now();
  long long duration =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time)
          .count();

  // Hard limit check FIRST
  if (duration >= hard_limit)
    return true;

  // Soft limit check with extension
  if (duration >= soft_limit * extension_factor) {
    return true;
  }

  return false;
}

} // namespace Search
} // namespace Prometheus
