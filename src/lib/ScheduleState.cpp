#include <ScheduleState.h>
#include <algorithm>
#include <numeric>

ScheduleState::ScheduleState(const JobShopInstance &instance)
    : instance(instance), schedule(instance),
      machine_next_available(instance.n_machines, 0),
      job_next_available(instance.n_jobs, 0),
      job_next_operation(instance.n_jobs, 0),
      job_work_remaining(instance.n_jobs),
      machine_work_remaining(instance.n_machines, 0) {
  for (int i = 0; i < instance.n_jobs; ++i) {
    job_work_remaining[i] = std::accumulate(
        instance.jobs[i].begin(), instance.jobs[i].end(), 0,
        [](int acc, const Operation &op) { return acc + op.duration; });
  }

  for (int i = 0; i < instance.n_jobs; ++i) {
    for (const auto &op : instance.jobs[i]) {
      machine_work_remaining[op.machine_id] += op.duration;
    }
  }

  for (int job_id = 0; job_id < instance.n_jobs; ++job_id) {
    int next_op = job_next_operation[job_id];
    if (next_op < instance.jobs[job_id].size()) {
      available_operations.insert(instance.jobs[job_id][next_op]);
    }
  }
}

void ScheduleState::dispatch(Operation op) {
  int start_time = std::max(machine_next_available[op.machine_id],
                            job_next_available[op.job_id]);
  int end_time = start_time + op.duration;
  auto scheduled_op = ScheduledOperation(op, start_time, end_time);
  schedule.add(scheduled_op);

  machine_next_available[op.machine_id] = end_time;
  job_next_available[op.job_id] = end_time;
  job_next_operation[op.job_id]++;
  job_work_remaining[op.job_id] -= op.duration;
  machine_work_remaining[op.machine_id] -= op.duration; // ADICIONADO

  available_operations.erase(op);
  if (job_next_operation[op.job_id] < instance.jobs[op.job_id].size()) {
    available_operations.insert(instance.jobs[op.job_id][job_next_operation[op.job_id]]);
  }
}
