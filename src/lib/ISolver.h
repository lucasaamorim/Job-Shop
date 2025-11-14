#pragma once

#include <ScheduleState.h>
#include <JobShopInstance.h>
#include <Schedule.h>
#include <chrono>

struct ISolver {
  JobShopInstance instance;

  ISolver(JobShopInstance instance)
    : instance(instance) {}

  /**
   * @brief Solves the scheduling problem.
   *
   * @param deadline The absolute time point at which the solver must stop.
   * @return A Schedule. If timed out, this may be an incomplete
   * or non-optimal schedule.
   */
  virtual Schedule solve(std::chrono::steady_clock::time_point deadline) = 0;

  virtual ~ISolver() = default;
};
