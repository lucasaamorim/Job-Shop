#pragma once

// TODO: Modify CMakeLists.txt so that you can include like this:
// #include <JobShopInstance.h>
// #include <ScheduledOperation.h>

#include "JobShopInstance.h"
#include "ScheduledOperation.h"

#include <algorithm>
#include <numeric>
#include <vector>

struct Schedule {
  JobShopInstance instance;
  std::vector<std::vector<ScheduledOperation>> schedule;

  constexpr static auto cmp_start_time = [](const ScheduledOperation &a,
                                            const ScheduledOperation &b) {
    return a.start_time < b.start_time;
  };

  Schedule(JobShopInstance instance)
      : instance(instance),
        schedule(instance.n_machines, std::vector<ScheduledOperation>()) {}

  void add(ScheduledOperation op);

  bool is_complete();

  int makespan();
};
