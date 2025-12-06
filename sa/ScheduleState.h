#pragma once

#include <JobShopInstance.h>
#include <Schedule.h>

#include <set>

struct ScheduleState {
  JobShopInstance instance;
  Schedule schedule;

  std::vector<int>
      machine_next_available; //!< Next available time for each machine
  std::vector<int> machine_work_remaining; //!< Duration of all unscheduled
                                           //!< operations for each machine

  std::vector<int> job_next_available;     //!< Next available time for each job

  std::vector<int>
      job_next_operation; //!< Next operation to be performed for each job

  // Maybe only when its a mkwr dispatcher?
  std::vector<int> job_work_remaining; //!< Duration of all unscheduled
                                       //!< operations for each job

  std::set<Operation>
      available_operations; //!< Operations available for dispatch

  ScheduleState(const JobShopInstance &inst);

  bool isDone() const { return available_operations.empty(); }

  void dispatch(Operation op);
};
