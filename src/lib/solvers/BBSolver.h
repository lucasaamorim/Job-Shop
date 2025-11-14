#pragma once

#include <ISolver.h>
#include <JobShopInstance.h>
#include <ScheduleState.h> // Needed for DispatchSolver in constructor

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
   * @return The best Schedule found.
   */
  Schedule solve() override;
};
