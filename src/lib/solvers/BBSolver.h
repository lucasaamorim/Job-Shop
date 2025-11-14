#pragma once

#include <ISolver.h>
#include <JobShopInstance.h>
#include <ScheduleState.h>
#include <chrono>

struct BBSolver : public ISolver {
  /**
   * @brief Stores the best schedule found so far.
   * Initialized with a heuristic solution.
   */
  Schedule best_schedule;

  /**
   * @brief The makespan of the best_schedule, stored for faster comparisons.
   */
  int upper_bound;

  BBSolver(const JobShopInstance &instance);

  /**
   * @brief Solves the Job Shop Scheduling problem using a
   * Branch and Bound algorithm with a priority queue (Best-First Search).
   *
   * This method will stop when the deadline is reached and return
   * the best complete schedule found up to that point.
   *
   * @param deadline The absolute time point at which the solver must stop.
   * @return The best Schedule found.
   */
  Schedule solve(std::chrono::steady_clock::time_point deadline) override;
};
