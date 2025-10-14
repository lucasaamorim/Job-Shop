#pragma once

#include <Dispatcher.h>
#include <JobShopInstance.h>
#include <Operation.h>
#include <Schedule.h>

#include <vector>

struct Dispatcher {
  JobShopInstance instance;
  Schedule schedule;

  std::vector<int>
      machine_next_available;          //!< Next available time for each machine
  std::vector<int> job_next_available; //!< Next available time for each job
  std::vector<int>
      job_next_operation; //!< Next operation to be performed for each job
  std::vector<int> job_work_remaining;

  Dispatcher(const JobShopInstance &instance);

  std::vector<Operation> available_operations();

  void dispatch(Operation op);
};
