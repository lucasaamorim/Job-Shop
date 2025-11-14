#include <solvers/DispatchSolver.h>
#include <chrono>

Schedule DispatchSolver::solve(std::chrono::steady_clock::time_point deadline) {
  ScheduleState state(instance);
  while (!state.schedule.is_complete()) {
    if (std::chrono::steady_clock::now() > deadline) {
      break;
    }
    Operation to_schedule = rule(state);
    state.dispatch(to_schedule);
  }
  return state.schedule;
}
