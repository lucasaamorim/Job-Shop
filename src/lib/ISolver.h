#pragma once

#include <ScheduleState.h>
#include <JobShopInstance.h>
#include <Schedule.h>

struct ISolver {
  JobShopInstance instance;

  ISolver(JobShopInstance instance)
    : instance(instance) {}

  virtual Schedule solve() = 0;

  virtual ~ISolver() = default;
};
