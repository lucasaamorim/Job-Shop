#pragma once

#include <ScheduleState.h>
#include <JobShopInstance.h>
#include <Rules.h>
#include <ISolver.h>

struct DispatchSolver : public ISolver {
  Rules::Rule rule;

  DispatchSolver(const JobShopInstance &instance,
                         const Rules::Rule &rule)
      : ISolver(instance), rule(rule) {}

  Schedule solve() override;
};
