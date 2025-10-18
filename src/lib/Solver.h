#pragma once

#include <Dispatcher.h>
#include <JobShopInstance.h>
#include <Schedule.h>

template<OperatorCompare Rule>
struct Solver {
  JobShopInstance instance;
  Dispatcher<Rule> dispatcher;

    Solver(const JobShopInstance &instance)
    : instance(instance) {}

  Schedule solve() {
    while (!dispatcher.schedule.is_complete()) dispatcher.dispatch();
    return dispatcher.schedule;
  }

};
