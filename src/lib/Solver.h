#pragma once

#include <JobShopInstance.h>
#include <Rules.h>
#include <Schedule.h>

struct Solver {
    Solver(JobShopInstance instance, Rules::Rule rule)
    : instance(std::move(instance)), rule(std::move(rule)) {}

  Schedule solve();

private:
  JobShopInstance instance;
  Rules::Rule rule;
};
