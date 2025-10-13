// TODO: Modify CMakeLists.txt so that you can include like this:
// #include <Dispatcher.h>
// #include <JobShopInstance.h>
// #include <Operation.h>
// #include <Rules.h>
// #include <Schedule.h>
#include "lib/Dispatcher.h"
#include "lib/JobShopInstance.h"
#include "lib/Operation.h"
#include "lib/Rules.h"
#include "lib/Schedule.h"
//TODO: Maybe make a Solver struct to bundle it all together and remove the includes above?

#include <functional>
#include <iostream>
#include <vector>

Schedule solve(JobShopInstance instance, Rules::Rule rule) {
  Dispatcher dispatcher(instance);
  while (not dispatcher.schedule.is_complete()) {
    Operation to_schedule = rule(dispatcher);

    dispatcher.dispatch(to_schedule);
  }
  return dispatcher.schedule;
}

int main() {
  int n_jobs, n_machines;
  std::cin >> n_jobs >> n_machines;

  std::vector<std::vector<Operation>> jobs(n_jobs,
                                           std::vector<Operation>(n_machines));
  for (int job_id = 0; job_id < n_jobs; ++job_id) {
    for (int op_idx = 0; op_idx < n_machines; ++op_idx) {
      int machine_id, duration;
      std::cin >> machine_id >> duration;
      jobs[job_id][op_idx] = Operation(
          {job_id, op_idx, machine_id, duration, job_id * n_machines + op_idx});
    }
  }

  JobShopInstance instance = JobShopInstance(jobs, n_machines);

  std::cout << "Solving Job Shop Instance with " << n_jobs << " jobs and "
            << n_machines << " machines." << std::endl;

  Schedule schedule = solve(instance, Rules::shortest_processing_time);

  std::cout << "Solution found!" << std::endl;

  std::cout << "Total Makespan: " << schedule.makespan() << std::endl;
}
