#include <bits/stdc++.h>

using namespace std;

struct Operation {
  int job_id;
  int position_in_job;
  int machine_id;
  int duration;
  int operation_id;
};

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

struct JobShopInstance {
  vector<vector<Operation>> jobs;
  int n_machines;
  int n_jobs;
  int n_operations;

  JobShopInstance(vector<vector<Operation>> jobs, int n_machines)
      : jobs(jobs), n_machines(n_machines), n_jobs(jobs.size()),
        n_operations(jobs.size() * jobs[0].size()) {}
};

struct Schedule {
  JobShopInstance instance;
  vector<vector<ScheduledOperation>> schedule;

  constexpr static auto cmp_start_time = [](const ScheduledOperation &a,
                                            const ScheduledOperation &b) {
    return a.start_time < b.start_time;
  };

  Schedule(JobShopInstance instance)
      : instance(instance),
        schedule(instance.n_machines, vector<ScheduledOperation>()) {}

  void add(ScheduledOperation op) {
    schedule[op.machine_id].push_back(op);
    sort(schedule[op.machine_id].begin(), schedule[op.machine_id].end(),
         cmp_start_time);
  }

  bool is_complete() {
    int n_scheduled =
        accumulate(schedule.begin(), schedule.end(), 0,
                   [](int acc, const vector<ScheduledOperation> &ops) {
                     return acc + ops.size();
                   });
    return n_scheduled == instance.n_operations;
  }

  int makespan() {
    if (instance.n_operations == 0)
      return 0;
    // assuming that each machine schedule is sorted by end time
    return max_element(schedule.begin(), schedule.end(),
                       [](const vector<ScheduledOperation> &a,
                          const vector<ScheduledOperation> &b) {
                         return a.back().end_time < b.back().end_time;
                       })
        ->back()
        .end_time;
  }
};

struct Dispatcher {
  JobShopInstance instance;
  Schedule schedule;

  vector<int> machine_next_available; //!< Next available time for each machine
  vector<int> job_next_available;     //!< Next available time for each job
  vector<int>
      job_next_operation; //!< Next operation to be performed for each job

  Dispatcher(JobShopInstance instance)
      : instance(instance), schedule(instance),
        machine_next_available(instance.n_machines),
        job_next_available(instance.n_jobs),
        job_next_operation(instance.n_jobs) {}

  vector<Operation> available_operations() {
    vector<Operation> available_ops;
    for (int job_id = 0; job_id < instance.n_jobs; ++job_id) {
      int next_op = job_next_operation[job_id];
      if (next_op < instance.jobs[job_id].size()) {
        available_ops.push_back(instance.jobs[job_id][next_op]);
      }
    }
    return available_ops;
  }

  void dispatch(Operation op) {
    int machine_id = op.machine_id;
    int start_time =
        max(machine_next_available[machine_id], job_next_available[op.job_id]);
    auto scheduled_op = ScheduledOperation(op, start_time, machine_id);
    schedule.add(scheduled_op);

    machine_next_available[machine_id] = scheduled_op.end_time;
    job_next_available[op.job_id] = scheduled_op.end_time;
    job_next_operation[op.job_id]++;
  }
};
Operation spt_rule(Dispatcher &dispatcher) {
  auto candidates = dispatcher.available_operations();
  return *min_element(candidates.begin(), candidates.end(),
                      [](const Operation &a, const Operation &b) {
                        return a.duration < b.duration;
                      });
}

Schedule solve(JobShopInstance instance) {
  Dispatcher dispatcher(instance);
  while (not dispatcher.schedule.is_complete()) {
    Operation to_schedule = spt_rule(dispatcher);

    dispatcher.dispatch(to_schedule);
  }
  return dispatcher.schedule;
}

int main() {
  int n_jobs, n_machines;
  cin >> n_jobs >> n_machines;

  vector<vector<Operation>> jobs(n_jobs, vector<Operation>(n_machines));
  for (int job_id = 0; job_id < n_jobs; ++job_id) {
      for (int op_idx = 0; op_idx < n_machines; ++op_idx) {
          int machine_id, duration;
          cin >> machine_id >> duration;
          jobs[job_id][op_idx] = Operation({job_id, op_idx, machine_id, duration, job_id*n_machines+op_idx});
      }
  }

  JobShopInstance instance = JobShopInstance(jobs, n_machines);

  cout << "Solving Job Shop Instance with " << n_jobs << " jobs and " << n_machines << " machines." << endl;

  Schedule schedule = solve(instance);

  cout << "Solution found!" << endl;

  cout << "Total Makespan: " << schedule.makespan() << endl;
}
