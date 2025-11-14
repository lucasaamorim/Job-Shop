#pragma once

#include <ScheduleState.h>
#include <JobShopInstance.h>
#include <Rules.h>
#include <ISolver.h>
#include <chrono>

struct DispatchSolver : public ISolver {
  Rules::Rule rule;

  DispatchSolver(const JobShopInstance &instance,
                         const Rules::Rule &rule)
      : ISolver(instance), rule(rule) {}

  Schedule solve(std::chrono::steady_clock::time_point deadline) override;
};
