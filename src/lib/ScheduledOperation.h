#pragma once
// TODO: Modify CMakeLists.txt so that you can include like this:
// #include <Operation.h>

#include "Operation.h"

struct ScheduledOperation {
  Operation operation;
  int start_time;
  int machine_id;
  int end_time;
  int job_id;

  ScheduledOperation(Operation op, int start_time, int machine_id)
      : operation(op), start_time(start_time), machine_id(machine_id),
        end_time(start_time + op.duration), job_id(op.job_id) {}
};
