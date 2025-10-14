#pragma once

#include <JobShopInstance.h>
#include <Rules.h>
#include <Schedule.h>

struct Solver {
    Solver(const JobShopInstance &instance, const Rules::Rule &rule)
    : instance(instance), rule(rule) {}

  Schedule solve();

private:
  JobShopInstance instance;
  Rules::Rule rule;
};
