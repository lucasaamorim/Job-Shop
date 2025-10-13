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

  void add(ScheduledOperation op) {
    schedule[op.machine_id].push_back(op);
    sort(schedule[op.machine_id].begin(), schedule[op.machine_id].end(),
         cmp_start_time);
  }

  bool is_complete() {
    int n_scheduled = std::accumulate(
        schedule.begin(), schedule.end(), 0,
        [](int acc, const std::vector<ScheduledOperation> &ops) {
          return acc + ops.size();
        });
    return n_scheduled == instance.n_operations;
  }

  int makespan() {
    if (instance.n_operations == 0)
      return 0;
    return max_element(schedule.begin(), schedule.end(),
                       [](const std::vector<ScheduledOperation> &a,
                          const std::vector<ScheduledOperation> &b) {
                         return a.back().end_time < b.back().end_time;
                       })
        ->back()
        .end_time;
  }
};
