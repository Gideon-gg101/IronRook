#include "time_manager.h"
#include <algorithm>
#include <iostream>

namespace Prometheus {
namespace Search {

TimeManager Timer;

// Helper: Calculate game phase based on piece count
static double calculate_game_phase(const Board &board) {
  int piece_count = Bitboards::popcount(board.all_pieces());
  // Opening/Midgame: 32-16 pieces → phase 0.0-0.5
  // Endgame: 16-6 pieces → phase 0.5-1.0
  if (piece_count >= 16) {
    return (32 - piece_count) / 32.0;
  } else {
    return 0.5 + (16 - std::max(6, piece_count)) / 20.0;
  }
}

// Helper: Get phase-based time allocation factor
static double get_phase_factor(double phase) {
  // Opening/Midgame (phase 0-0.5): 1/20 fast play
  // Transitional (phase 0.5-0.7): 1/15 more time
  // Endgame (phase 0.7-1.0): 1/10 precise calculation
  if (phase < 0.5) {
    return 1.0 / 20.0;
  } else if (phase < 0.7) {
    return 1.0 / 15.0;
  } else {
    return 1.0 / 10.0;
  }
}

// Helper: Gradual low-time factor
static double low_time_factor(int time_left, int original_time_control) {
  if (original_time_control <= 0) {
    return 1.0 / 40.0; // Default conservative
  }

  double ratio = (double)time_left / original_time_control;
  if (ratio > 0.5) {
    return 1.0; // No scaling
  } else if (ratio > 0.1) {
    // Linear interpolation from 1.0 → 0.5 as time decreases
    double t = (ratio - 0.1) / 0.4;
    return 1.0 * t + 0.5 * (1 - t);
  } else {
    return 0.5; // Very conservative
  }
}

int TimeManager::smooth_time_allocation(int raw_allocation) {
  if (last_allocated_time == 0) {
    last_allocated_time = raw_allocation;
    return raw_allocation;
  }

  // EMA: smoothed = alpha * current + (1-alpha) * previous
  int smoothed = (int)(smoothing_alpha * raw_allocation +
                       (1 - smoothing_alpha) * last_allocated_time);
  last_allocated_time = smoothed;
  return smoothed;
}

void TimeManager::init(const SearchLimits &limits, Color sideToMove,
                       const Board &board) {
  start_time = std::chrono::high_resolution_clock::now();
  nodes_since_check = 0;
  infinite = limits.infinite;
  extension_factor = 1.0;
  volatility = 0.0;

  if (infinite || limits.depth > 0 || limits.nodes > 0) {
    soft_limit = 2000000000; // Effectively infinite
    hard_limit = 2000000000;
    if (limits.ponder)
      infinite = true;
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

  // Phase-based allocation with endgame awareness
  double game_phase = calculate_game_phase(board);
  double base_factor = get_phase_factor(game_phase);

  // Apply low-time scaling
  int original_tc = time_left + inc * 40; // Estimate original time control
  double lt_factor = low_time_factor(time_left, original_tc);

  // Calculate raw allocation
  int raw_allocation = (int)(time_left * base_factor * lt_factor) + inc;

  // Apply time smoothing
  soft_limit = smooth_time_allocation(raw_allocation);

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

  if (limits.ponder)
    infinite = true;
}

void TimeManager::on_ponderhit() {
  start_time = std::chrono::high_resolution_clock::now();
  infinite = false;
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

void TimeManager::update_best_move(Move m, int depth) {
  if (m != last_best_move) {
    last_best_move = m;
    stability_counter = 0;

    // Volatility: Change at higher depth implies instability
    // Formula: accumulated volatility += depth * depth
    volatility += depth * depth;

    // User Formula: time *= 1 + min(0.6, volatility / 200.0);
    double vol_factor = 1.0 + std::min(0.6, volatility / 200.0);
    extend_time(vol_factor);

  } else {
    stability_counter++;
  }
}

void TimeManager::fail_low() { extend_time(1.0 + fail_low_extension); }

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

long long TimeManager::elapsed() const {
  auto now = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - start_time)
      .count();
}

} // namespace Search
} // namespace Prometheus
