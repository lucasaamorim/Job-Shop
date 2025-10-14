#include <Dispatcher.h>
#include <numeric>
#include <algorithm>

Dispatcher::Dispatcher(const JobShopInstance &instance)
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

std::vector<Operation> Dispatcher::available_operations() {
  std::vector<Operation> available_ops;
  for (int job_id = 0; job_id < instance.n_jobs; ++job_id) {
    int next_op = job_next_operation[job_id];
    if (next_op < instance.jobs[job_id].size()) {
      available_ops.push_back(instance.jobs[job_id][next_op]);
    }
  }
  return available_ops;
}

void Dispatcher::dispatch(Operation op) {
  int machine_id = op.machine_id;
  int start_time = std::max(machine_next_available[machine_id],
                            job_next_available[op.job_id]);
  auto scheduled_op = ScheduledOperation(op, start_time, machine_id);
  schedule.add(scheduled_op);

  machine_next_available[machine_id] = scheduled_op.end_time;
  job_next_available[op.job_id] = scheduled_op.end_time;
  job_next_operation[op.job_id]++;
  job_work_remaining[op.job_id] -= op.duration;
}
