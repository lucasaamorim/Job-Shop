#pragma once

#include <JobShopInstance.h>
#include <Operation.h>
#include <Schedule.h>

#include <vector>
#include <set>
#include <numeric>
#include <concepts>
#include <type_traits>

template<typename Rule>
concept OperatorCompare =
std::predicate<Rule, const Operation&, const Operation&> and std::is_nothrow_copy_constructible_v<Rule>;

template <OperatorCompare Rule> struct Dispatcher {
  typedef std::set<Operation, Rule> ruled_set;

  JobShopInstance instance;
  Schedule schedule;

  std::vector<int>
      machine_next_available;          //!< Next available time for each machine
  std::vector<int> job_next_available; //!< Next available time for each job
  std::vector<int>
      job_next_operation; //!< Next operation to be performed for each job
  std::vector<int> job_work_remaining;
  ruled_set candidates;

  Dispatcher(const JobShopInstance &instance)
      : instance(instance), schedule(instance),
        machine_next_available(instance.n_machines),
        job_next_available(instance.n_jobs),
        job_next_operation(instance.n_jobs),
        job_work_remaining(instance.n_jobs) {
    for (int i = 0; i < instance.n_jobs; ++i) {
      job_work_remaining[i] = std::accumulate(
          instance.jobs[i].begin(), instance.jobs[i].end(), 0,
          [](int acc, const Operation &op) { return acc + op.duration; });
    }
  }

  //TODO: Remove this function and fetch candidates upon construction
  ruled_set available_operations() {
    std::vector<Operation> available_ops;
    for (int job_id = 0; job_id < instance.n_jobs; ++job_id) {
      int next_op = job_next_operation[job_id];
      if (next_op < instance.jobs[job_id].size()) {
        available_ops.push_back(instance.jobs[job_id][next_op]);
      }
    }
    return available_ops;
  }

  void dispatch() {
    const Operation &op = *candidates.begin();
    int machine_id = op.machine_id;
    int start_time = std::max(machine_next_available[machine_id],
                              job_next_available[op.job_id]);
    auto scheduled_op = ScheduledOperation(op, start_time, machine_id);
    schedule.add(scheduled_op);

    candidates.erase(op);
    //TODO: Insert the new candidate here
    machine_next_available[machine_id] = scheduled_op.end_time;
    job_next_available[op.job_id] = scheduled_op.end_time;
    job_next_operation[op.job_id]++;
    job_work_remaining[op.job_id] -= op.duration;
  }
};
