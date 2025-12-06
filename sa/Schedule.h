#pragma once

#include "JobShopInstance.h"

#include <set>
#include <vector>

struct Schedule {
  JobShopInstance instance;
  struct cmp_start_time {
    bool operator()(const ScheduledOperation &a,
                    const ScheduledOperation &b) const {
      if (a.start_time != b.start_time) {
        return a.start_time < b.start_time;
      }
      // tie breaker
      return a.operation_id < b.operation_id;
    }
  };

  typedef std::set<ScheduledOperation, cmp_start_time> op_set;

  std::vector<op_set> schedule;

  Schedule(JobShopInstance instance)
      : instance(instance), schedule(instance.n_machines, op_set()) {}

  void add(ScheduledOperation op);

  bool is_complete();

  int makespan() const;
};
