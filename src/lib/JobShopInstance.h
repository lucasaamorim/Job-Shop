#pragma once

#include <Operation.h>

#include <vector>

struct JobShopInstance {
  std::vector<std::vector<Operation>> jobs;
  int n_machines;
  int n_jobs;
  int n_operations;

  JobShopInstance(std::vector<std::vector<Operation>> jobs, int n_machines)
      : jobs(jobs), n_machines(n_machines), n_jobs(jobs.size()),
        n_operations(jobs.size() * jobs[0].size()) {}
};
