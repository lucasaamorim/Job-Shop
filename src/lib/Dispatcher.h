#pragma once

// TODO: Modify CMakeLists.txt so that you can include like this:
// #include <Dispatcher.h>
// #include <JobShopInstance.h>
// #include <Operation.h>
// #include <Schedule.h>
#include "Dispatcher.h"
#include "JobShopInstance.h"
#include "Operation.h"
#include "Schedule.h"

#include <vector>

struct Dispatcher {
  JobShopInstance instance;
  Schedule schedule;

  std::vector<int>
      machine_next_available;          //!< Next available time for each machine
  std::vector<int> job_next_available; //!< Next available time for each job
  std::vector<int>
      job_next_operation; //!< Next operation to be performed for each job

  Dispatcher(JobShopInstance instance)
      : instance(instance), schedule(instance),
        machine_next_available(instance.n_machines),
        job_next_available(instance.n_jobs),
        job_next_operation(instance.n_jobs) {}

  std::vector<Operation> available_operations() {
    std::vector<Operation> available_ops;
    for (int job_id = 0; job_id < instance.n_jobs; ++job_id) {
      int next_op = job_next_operation[job_id];
      if (next_op < instance.jobs[job_id].size()) {
        available_ops.push_back(instance.jobs[job_id][next_op]);
      }
    }
    return available_ops;
  }

  void dispatch(Operation op) {
    int machine_id = op.machine_id;
    int start_time = std::max(machine_next_available[machine_id],
                              job_next_available[op.job_id]);
    auto scheduled_op = ScheduledOperation(op, start_time, machine_id);
    schedule.add(scheduled_op);

    machine_next_available[machine_id] = scheduled_op.end_time;
    job_next_available[op.job_id] = scheduled_op.end_time;
    job_next_operation[op.job_id]++;
  }
};
