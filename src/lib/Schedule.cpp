#include <Schedule.h>

#include <algorithm>
#include <numeric>

void Schedule::add(ScheduledOperation op) {
  schedule[op.machine_id].push_back(op);
  sort(schedule[op.machine_id].begin(), schedule[op.machine_id].end(),
       cmp_start_time);
}

bool Schedule::is_complete() {
  int n_scheduled =
      std::accumulate(schedule.begin(), schedule.end(), 0,
                      [](int acc, const std::vector<ScheduledOperation> &ops) {
                        return acc + ops.size();
                      });
  return n_scheduled == instance.n_operations;
}

int Schedule::makespan() {
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
