#include <solvers/DispatchSolver.h>

Schedule DispatchSolver::solve() {
  ScheduleState state(instance);
  while (!state.schedule.is_complete()) {
    Operation to_schedule = rule(state);
    state.dispatch(to_schedule);
  }
  return state.schedule;
}
