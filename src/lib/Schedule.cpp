#include <Schedule.h>

#include <algorithm>
#include <numeric>

void Schedule::add(ScheduledOperation op) {
    schedule[op.machine_id].insert(op);
}

bool Schedule::is_complete() {
  int n_scheduled =
      std::accumulate(schedule.begin(), schedule.end(), 0,
                      [](int acc, const op_set &ops) {
                        return acc + ops.size();
                      });
  return n_scheduled == instance.n_operations;
}

int Schedule::makespan() {
  if (instance.n_operations == 0)
    return 0;
  return max_element(schedule.begin(), schedule.end(),
                     [](const op_set &a,
                        const op_set &b) {
                       return a.rbegin()->end_time < b.rbegin()->end_time;
                     })
      ->rbegin()
      ->end_time;
}
