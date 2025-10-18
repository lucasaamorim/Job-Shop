#include <Schedule.h>

#include <algorithm>

void Schedule::add(ScheduledOperation op) {
    schedule[op.machine_id].insert(op);
    n_scheduled++;
}

bool Schedule::is_complete() {
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
