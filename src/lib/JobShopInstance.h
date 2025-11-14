#pragma once

#include <vector>

/// Stores data about an Operation
struct Operation {
  int job_id;
  int machine_id;
  int operation_id;
  int position_in_job;
  int duration;

  inline auto operator<=>(const Operation &other) const {
    return operation_id <=> other.operation_id;
  }
};

/// Stores data about an Scheduled Operation
struct ScheduledOperation : public Operation {
  int start_time;
  int end_time;

  ScheduledOperation(Operation op, int start, int end)
      : Operation(op), start_time(start), end_time(end) {}

  ScheduledOperation(int jid, int mid, int opid,
                     int pos, int dur, int start,
                     int end)
      : Operation(jid, mid, opid, pos, dur), start_time(start), end_time(end) {}
};

/// Stores data about an instance of a job-shop scheduling problem
struct JobShopInstance {
  std::vector<std::vector<Operation>> jobs;
  int n_jobs;
  int n_machines;
  int n_operations;

  JobShopInstance(std::vector<std::vector<Operation>> j, int nj,
                  int nm, int no)
      : jobs(j), n_jobs(nj), n_machines(nm), n_operations(no) {}
};
