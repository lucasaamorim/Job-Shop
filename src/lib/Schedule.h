#pragma once

// TODO: Modify CMakeLists.txt so that you can include like this:
// #include <JobShopInstance.h>
// #include <ScheduledOperation.h>

#include <JobShopInstance.h>
#include <ScheduledOperation.h>

#include <vector>
#include <set>

struct Schedule {
  JobShopInstance instance;
  struct cmp_start_time {
      bool operator()(const ScheduledOperation &a, const ScheduledOperation &b) const {
        if (a.start_time != b.start_time) {
          return a.start_time < b.start_time;
        }
        // tie breaker
        return a.operation.operation_id < b.operation.operation_id;
      }
    };

  typedef std::set<ScheduledOperation, cmp_start_time> op_set;

  std::vector<op_set> schedule;

  size_t n_scheduled;

  Schedule(JobShopInstance instance)
      : instance(instance),
        schedule(instance.n_machines, op_set()),
       n_scheduled(0) {}

  void add(ScheduledOperation op);

  bool is_complete();

  int makespan();
};
