#include <Solver.h>
#include <Dispatcher.h>

Schedule Solver::solve() {
  Dispatcher dispatcher(instance);
  while (!dispatcher.schedule.is_complete()) {
    Operation to_schedule = rule(dispatcher);
    dispatcher.dispatch(to_schedule);
  }
  return dispatcher.schedule;
}
