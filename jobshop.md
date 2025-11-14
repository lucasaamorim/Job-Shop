Project Path: src

Source Tree:

```txt
src
├── lib
│   ├── ISolver.h
│   ├── JobShopInstance.h
│   ├── Rules.h
│   ├── Schedule.cpp
│   ├── Schedule.h
│   ├── ScheduleState.cpp
│   ├── ScheduleState.h
│   └── solvers
│       ├── BBSolver.cpp
│       ├── BBSolver.h
│       ├── DispatchSolver.cpp
│       └── DispatchSolver.h
└── main.cpp

```

`src/lib/ISolver.h`:

```h
#pragma once

#include <ScheduleState.h>
#include <JobShopInstance.h>
#include <Schedule.h>
#include <chrono>

struct ISolver {
  JobShopInstance instance;

  ISolver(JobShopInstance instance)
    : instance(instance) {}

  /**
   * @brief Solves the scheduling problem.
   *
   * @param deadline The absolute time point at which the solver must stop.
   * @return A Schedule. If timed out, this may be an incomplete
   * or non-optimal schedule.
   */
  virtual Schedule solve(std::chrono::steady_clock::time_point deadline) = 0;

  virtual ~ISolver() = default;
};

```

`src/lib/JobShopInstance.h`:

```h
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

```

`src/lib/Rules.h`:

```h
#pragma once

#include <ScheduleState.h>
#include <JobShopInstance.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <random>

namespace Rules {
typedef std::function<Operation(ScheduleState &)> Rule;

inline const Rule shortest_processing_time =
    [](ScheduleState &state) -> Operation {
  auto candidates = state.available_operations;
  return *std::min_element(candidates.begin(), candidates.end(),
                           [](const Operation &a, const Operation &b) {
                             return a.duration < b.duration;
                           });
};

inline const Rule most_work_remaining =
    [](ScheduleState &state) -> Operation {
  auto candidates = state.available_operations;
  return *std::max_element(
      candidates.begin(), candidates.end(),
      [&state](const Operation &a, const Operation &b) {
        return state.job_work_remaining[a.job_id] <
               state.job_work_remaining[b.job_id];
      }

  );
};

inline const Rule first_come_first_served =
    [](ScheduleState &state) -> Operation {
  auto candidates = state.available_operations;
  return *min_element(candidates.begin(), candidates.end(),
                      [](const Operation &a, const Operation &b) {
                        return a.position_in_job < b.position_in_job;
                      });
};

//TODO: Reutilize the generator instead of reinstantiating it every call
inline const Rule random_operation = [](ScheduleState &state) -> Operation {
  auto candidates = state.available_operations;
  std::random_device rd;
  std::mt19937 gen(rd());

  std::uniform_int_distribution<> distrib(0, candidates.size()-1);
  auto it = candidates.begin();
  std::advance(it, distrib(gen));

  return *it;
};

inline const Rule longest_processing_time =
    [](ScheduleState &state) -> Operation {
  auto candidates = state.available_operations;
  return *std::max_element(candidates.begin(), candidates.end(),
                           [](const Operation &a, const Operation &b) {
                             return a.duration < b.duration;
                           });
};
} // namespace Rules

```

`src/lib/Schedule.cpp`:

```cpp
#include <Schedule.h>

#include <algorithm>
#include <numeric>

void Schedule::add(ScheduledOperation op) {
    schedule[op.machine_id].insert(op);
}

bool Schedule::is_complete() {
  int n_scheduled =
      std::accumulate(schedule.begin(), schedule.end(), 0,
                      [](int acc, const op_set &ops) {
                        return acc + ops.size();
                      });
  return n_scheduled == instance.n_operations;
}

int Schedule::makespan() const{
  // O 'makespan' geral começa em 0
  int max_makespan = 0;

  // Itera por cada máquina (cada op_set no vetor)
  for (const auto &machine_schedule : schedule) {
    // Verifica se o set da máquina NÃO está vazio
    if (!machine_schedule.empty()) {
      // Pega o end_time da última operação dessa máquina
      int machine_end_time = machine_schedule.rbegin()->end_time;

      // Atualiza o makespan geral se o desta máquina for maior
      if (machine_end_time > max_makespan) {
        max_makespan = machine_end_time;
      }
    }
    // Se machine_schedule estiver vazio, seu makespan é 0,
    // então ele é ignorado (corretamente) pelo 'if'.
  }

  return max_makespan;
}

```

`src/lib/Schedule.h`:

```h
#pragma once

#include <JobShopInstance.h>

#include <set>
#include <vector>

struct Schedule {
  JobShopInstance instance;
  struct cmp_start_time {
    bool operator()(const ScheduledOperation &a,
                    const ScheduledOperation &b) const {
      if (a.start_time != b.start_time) {
        return a.start_time < b.start_time;
      }
      // tie breaker
      return a.operation_id < b.operation_id;
    }
  };

  typedef std::set<ScheduledOperation, cmp_start_time> op_set;

  std::vector<op_set> schedule;

  Schedule(JobShopInstance instance)
      : instance(instance), schedule(instance.n_machines, op_set()) {}

  void add(ScheduledOperation op);

  bool is_complete();

  int makespan() const;
};

```

`src/lib/ScheduleState.cpp`:

```cpp
#include <ScheduleState.h>
#include <algorithm>
#include <numeric>

ScheduleState::ScheduleState(const JobShopInstance &instance)
    : instance(instance), schedule(instance),
      machine_next_available(instance.n_machines, 0),
      job_next_available(instance.n_jobs, 0),
      job_next_operation(instance.n_jobs, 0),
      job_work_remaining(instance.n_jobs),
      machine_work_remaining(instance.n_machines, 0) {
  for (int i = 0; i < instance.n_jobs; ++i) {
    job_work_remaining[i] = std::accumulate(
        instance.jobs[i].begin(), instance.jobs[i].end(), 0,
        [](int acc, const Operation &op) { return acc + op.duration; });
  }

  for (int i = 0; i < instance.n_jobs; ++i) {
    for (const auto &op : instance.jobs[i]) {
      machine_work_remaining[op.machine_id] += op.duration;
    }
  }

  for (int job_id = 0; job_id < instance.n_jobs; ++job_id) {
    int next_op = job_next_operation[job_id];
    if (next_op < instance.jobs[job_id].size()) {
      available_operations.insert(instance.jobs[job_id][next_op]);
    }
  }
}

void ScheduleState::dispatch(Operation op) {
  int start_time = std::max(machine_next_available[op.machine_id],
                            job_next_available[op.job_id]);
  int end_time = start_time + op.duration;
  auto scheduled_op = ScheduledOperation(op, start_time, end_time);
  schedule.add(scheduled_op);

  machine_next_available[op.machine_id] = end_time;
  job_next_available[op.job_id] = end_time;
  job_next_operation[op.job_id]++;
  job_work_remaining[op.job_id] -= op.duration;
  machine_work_remaining[op.machine_id] -= op.duration; // ADICIONADO

  available_operations.erase(op);
  if (job_next_operation[op.job_id] < instance.jobs[op.job_id].size()) {
    available_operations.insert(instance.jobs[op.job_id][job_next_operation[op.job_id]]);
  }
}

```

`src/lib/ScheduleState.h`:

```h
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

```

`src/lib/solvers/BBSolver.cpp`:

```cpp
#include <Rules.h>
#include <algorithm>
#include <climits>
#include <memory>
#include <optional>
#include <queue>
#include <solvers/BBSolver.h>
#include <solvers/DispatchSolver.h>
#include <vector>

namespace {

struct Node {
  std::vector<int> next_op_index;
  std::vector<int> machine_free_time;
  std::vector<int> job_completion_time;
  int current_makespan;
  int lower_bound;

  std::vector<int> lb_per_job;     // LB contribution from each Job
  std::vector<int> lb_per_machine; // LB contribution from each Machine

  std::shared_ptr<const Node> parent;
  std::optional<ScheduledOperation> scheduled_op;

  bool operator>(const Node &other) const {
    return lower_bound > other.lower_bound;
  }
};

struct NodeComparator {
  bool operator()(const std::shared_ptr<const Node> &a,
                  const std::shared_ptr<const Node> &b) const {
    return a->lower_bound > b->lower_bound;
  }
};

struct OperationDataToSingleMachine {
  int job_id;
  int duration;
  int release_time;
};

struct ReleaseTimeComparator {
  bool operator()(const OperationDataToSingleMachine &a,
                  const OperationDataToSingleMachine &b) const {
    return a.release_time > b.release_time;
  }
};

struct ProcessingTimeComparator {
  bool operator()(const OperationDataToSingleMachine &a,
                  const OperationDataToSingleMachine &b) const {
    if (a.duration != b.duration)
      return a.duration > b.duration;
    return a.release_time > b.release_time;
  }
};

// Helper to calculate r_ij
int calculate_longest_path_U_to_O(const Node &node, const Operation &op) {
  int pred_time =
      (op.position_in_job > 0) ? node.job_completion_time[op.job_id] : 0;
  return std::max(pred_time, node.machine_free_time[op.machine_id]);
}

int solve_1_rj_cmax(
    const std::vector<OperationDataToSingleMachine> &machine_ops) {
  if (machine_ops.empty())
    return 0;

  std::priority_queue<OperationDataToSingleMachine,
                      std::vector<OperationDataToSingleMachine>,
                      ReleaseTimeComparator>
      not_released;
  std::priority_queue<OperationDataToSingleMachine,
                      std::vector<OperationDataToSingleMachine>,
                      ProcessingTimeComparator>
      ready;

  for (const auto &op : machine_ops)
    not_released.push(op);

  int current_time = 0;
  int max_completion = 0;

  while (!not_released.empty() || !ready.empty()) {
    while (!not_released.empty() &&
           not_released.top().release_time <= current_time) {
      ready.push(not_released.top());
      not_released.pop();
    }

    if (!ready.empty()) {
      auto op = ready.top();
      ready.pop();
      int start = std::max(current_time, op.release_time);
      current_time = start + op.duration;
      max_completion = std::max(max_completion, current_time);
    } else if (!not_released.empty()) {
      current_time = not_released.top().release_time;
    }
  }
  return max_completion;
}

int compute_machine_lb_component(const Node &node,
                                 const JobShopInstance &instance,
                                 int machine_id) {
  std::vector<OperationDataToSingleMachine> ops;
  // We must scan all jobs to find operations pending for this machine
  for (int j = 0; j < instance.n_jobs; ++j) {
    int next_idx = node.next_op_index[j];
    // Look ahead in this job to find if/when it uses 'machine_id'
    // NOTE: Strictly speaking, the 1|rj|Cmax bound considers ALL future ops on
    // this machine. However, a common relaxation is to consider only the
    // *immediately available* ones or the ones that will become available. For
    // strict correctness, we find the *next* operation for this machine in this
    // job.

    for (size_t k = next_idx; k < instance.jobs[j].size(); ++k) {
      const auto &op = instance.jobs[j][k];
      if (op.machine_id == machine_id) {
        // Release time approximation:
        // It can't start before the previous op in this job finishes.
        // If it is the IMMEDIATE next op (k == next_idx), we know the exact
        // ready time from job_completion_time. If it is further in the future,
        // we add the durations of intermediate ops.

        int r_j = node.job_completion_time[j];
        for (size_t p = next_idx; p < k; ++p) {
          r_j += instance.jobs[j][p].duration;
        }

        // Also bounded by machine free time (implicit in 1|rj|cmax, but we can
        // clamp r_j)
        r_j = std::max(r_j, node.machine_free_time[machine_id]);

        ops.push_back({op.job_id, op.duration, r_j});
      }
    }
  }
  return solve_1_rj_cmax(ops);
}

int compute_job_lb_component(const Node &node, const JobShopInstance &instance,
                             int job_id) {
  int remaining = 0;
  for (size_t k = node.next_op_index[job_id]; k < instance.jobs[job_id].size();
       ++k) {
    remaining += instance.jobs[job_id][k].duration;
  }
  return node.job_completion_time[job_id] + remaining;
}

/**
 * Full calculation for the root node.
 */
void initialize_lower_bound(Node &node, const JobShopInstance &instance) {
  node.lb_per_job.resize(instance.n_jobs);
  node.lb_per_machine.resize(instance.n_machines);

  int max_lb = node.current_makespan;

  // 1. Compute Job LBs
  for (int j = 0; j < instance.n_jobs; ++j) {
    node.lb_per_job[j] = compute_job_lb_component(node, instance, j);
    max_lb = std::max(max_lb, node.lb_per_job[j]);
  }

  // 2. Compute Machine LBs
  for (int m = 0; m < instance.n_machines; ++m) {
    node.lb_per_machine[m] = compute_machine_lb_component(node, instance, m);
    max_lb = std::max(max_lb, node.lb_per_machine[m]);
  }

  node.lower_bound = max_lb;
}

/**
 * INCREMENTAL UPDATE
 * Only updates the specific job and machine affected by the scheduled
 * operation. NOTE: Updating a job might affect release times for OTHER machines
 * slightly, but in many B&B implementations, we accept the "staleness" of other
 * machine LBs or only update the critical machine to save time.
 * * To be mathematically rigorous: Changing job J's completion time updates r_j
 * for ALL machines that have future operations from job J.
 */
void update_lower_bound_incremental(Node &node, const JobShopInstance &instance,
                                    const Operation &scheduled_op) {
  int max_lb = node.current_makespan;

  // 1. Update the specific Job LB
  node.lb_per_job[scheduled_op.job_id] =
      compute_job_lb_component(node, instance, scheduled_op.job_id);

  // 2. Update the specific Machine LB (The one that just got busy)
  node.lb_per_machine[scheduled_op.machine_id] =
      compute_machine_lb_component(node, instance, scheduled_op.machine_id);

  // 3. (Rigorous Step) Update Machine LBs for any future ops of this job
  // Because Job J finished later, its future ops now have later release times,
  // potentially pushing back the LB for the machines those ops use.
  for (size_t k = node.next_op_index[scheduled_op.job_id];
       k < instance.jobs[scheduled_op.job_id].size(); ++k) {
    int m_id = instance.jobs[scheduled_op.job_id][k].machine_id;
    if (m_id != scheduled_op.machine_id) {
      node.lb_per_machine[m_id] =
          compute_machine_lb_component(node, instance, m_id);
    }
  }

  // 4. Aggregate
  for (int lb : node.lb_per_job)
    max_lb = std::max(max_lb, lb);
  for (int lb : node.lb_per_machine)
    max_lb = std::max(max_lb, lb);

  node.lower_bound = max_lb;
}

} // namespace

// --- BBSolver Implementation ---

BBSolver::BBSolver(const JobShopInstance &instance)
    : ISolver(instance),
      // Initialize with a heuristic solution (SPT, as used in Teste...)
      best_schedule(Schedule(instance)) {
  Schedule spt = DispatchSolver(instance, Rules::shortest_processing_time)
                     .solve(std::chrono::steady_clock::time_point::max());
  Schedule mwr = DispatchSolver(instance, Rules::most_work_remaining)
                     .solve(std::chrono::steady_clock::time_point::max());
  Schedule fcfs = DispatchSolver(instance, Rules::first_come_first_served)
                      .solve(std::chrono::steady_clock::time_point::max());
  Schedule lpt = DispatchSolver(instance, Rules::longest_processing_time)
                     .solve(std::chrono::steady_clock::time_point::max());
  Schedule random = DispatchSolver(instance, Rules::random_operation)
                        .solve(std::chrono::steady_clock::time_point::max());

  best_schedule = std::min({spt, mwr, fcfs, lpt /*, random*/},
                           [](const Schedule &a, const Schedule &b) -> bool {
                             return a.makespan() < b.makespan();
                           });
  // Store the makespan for faster comparisons
  upper_bound = best_schedule.makespan();
}

Schedule BBSolver::solve(std::chrono::steady_clock::time_point deadline) {
  std::priority_queue<std::shared_ptr<const Node>,
                      std::vector<std::shared_ptr<const Node>>, NodeComparator>
      queue;

  // 1. Initialize root node
  Node root;
  root.next_op_index.assign(instance.n_jobs, 0);
  root.machine_free_time.assign(instance.n_machines, 0);
  root.job_completion_time.assign(instance.n_jobs, 0);
  root.current_makespan = 0;

  // FULL CALCULATION FOR ROOT
  initialize_lower_bound(root, instance);

  auto root_node_ptr = std::make_shared<Node>(root);
  queue.push(root_node_ptr);

  while (!queue.empty()) {
    if (std::chrono::steady_clock::now() > deadline) {
      break; // Time limit reached, return the best solution found so far
    }

    auto current_node_ptr = queue.top();
    queue.pop();

    if (current_node_ptr->lower_bound >= upper_bound)
      continue;

    // Goal Check
    bool all_jobs_finished = true;
    for (int job = 0; job < instance.n_jobs; job++) {
      if (current_node_ptr->next_op_index[job] < instance.jobs[job].size()) {
        all_jobs_finished = false;
        break;
      }
    }

    if (all_jobs_finished) {
      if (current_node_ptr->current_makespan < upper_bound) {
        upper_bound = current_node_ptr->current_makespan;
        best_schedule = Schedule(instance);
        std::vector<ScheduledOperation> ops;
        auto trace_ptr = current_node_ptr;
        while (trace_ptr->parent) {
          ops.push_back(trace_ptr->scheduled_op.value());
          trace_ptr = trace_ptr->parent;
        }
        std::reverse(ops.begin(), ops.end());
        for (const auto &op : ops)
          best_schedule.add(op);
      }
      continue;
    }

    // Branching
    std::vector<Operation> eligible_ops;
    for (int job = 0; job < instance.n_jobs; ++job) {
      if (current_node_ptr->next_op_index[job] < instance.jobs[job].size()) {
        eligible_ops.push_back(
            instance.jobs[job][current_node_ptr->next_op_index[job]]);
      }
    }

    int min_completion_time = INT_MAX;
    int critical_machine_id = -1;

    for (const auto &op : eligible_ops) {
      int r_ij = std::max(current_node_ptr->job_completion_time[op.job_id],
                          current_node_ptr->machine_free_time[op.machine_id]);
      int c_ij = r_ij + op.duration;
      if (c_ij < min_completion_time) {
        min_completion_time = c_ij;
        critical_machine_id = op.machine_id;
      }
    }

    for (const auto &op_to_schedule : eligible_ops) {
      if (op_to_schedule.machine_id == critical_machine_id) {

        auto new_node_ptr = std::make_shared<Node>(*current_node_ptr);

        // *** FIX: Set Parent ***
        new_node_ptr->parent = current_node_ptr;

        int start_time =
            std::max(new_node_ptr->machine_free_time[op_to_schedule.machine_id],
                     new_node_ptr->job_completion_time[op_to_schedule.job_id]);
        int completion_time = start_time + op_to_schedule.duration;

        new_node_ptr->machine_free_time[op_to_schedule.machine_id] =
            completion_time;
        new_node_ptr->job_completion_time[op_to_schedule.job_id] =
            completion_time;
        new_node_ptr->current_makespan =
            std::max(new_node_ptr->current_makespan, completion_time);
        new_node_ptr->next_op_index[op_to_schedule.job_id]++;
        new_node_ptr->scheduled_op =
            ScheduledOperation(op_to_schedule, start_time, completion_time);

        // *** OPTIMIZATION: Incremental LB Update ***
        // Instead of calculate_lower_bound(...), we call:
        update_lower_bound_incremental(*new_node_ptr, instance, op_to_schedule);

        if (new_node_ptr->lower_bound < upper_bound) {
          queue.push(new_node_ptr);
        }
      }
    }
  }
  return best_schedule;
}

```

`src/lib/solvers/BBSolver.h`:

```h
#pragma once

#include <ISolver.h>
#include <JobShopInstance.h>
#include <ScheduleState.h>
#include <chrono>

struct BBSolver : public ISolver {
  /**
   * @brief Stores the best schedule found so far.
   * Initialized with a heuristic solution.
   */
  Schedule best_schedule;

  /**
   * @brief The makespan of the best_schedule, stored for faster comparisons.
   */
  int upper_bound;

  BBSolver(const JobShopInstance &instance);

  /**
   * @brief Solves the Job Shop Scheduling problem using a
   * Branch and Bound algorithm with a priority queue (Best-First Search).
   *
   * This method will stop when the deadline is reached and return
   * the best complete schedule found up to that point.
   *
   * @param deadline The absolute time point at which the solver must stop.
   * @return The best Schedule found.
   */
  Schedule solve(std::chrono::steady_clock::time_point deadline) override;
};

```

`src/lib/solvers/DispatchSolver.cpp`:

```cpp
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

```

`src/lib/solvers/DispatchSolver.h`:

```h
#pragma once

#include <ScheduleState.h>
#include <JobShopInstance.h>
#include <Rules.h>
#include <ISolver.h>
#include <chrono>

struct DispatchSolver : public ISolver {
  Rules::Rule rule;

  DispatchSolver(const JobShopInstance &instance,
                         const Rules::Rule &rule)
      : ISolver(instance), rule(rule) {}

  Schedule solve(std::chrono::steady_clock::time_point deadline) override;
};

```

`src/main.cpp`:

```cpp
#include <ISolver.h>
#include <JobShopInstance.h>
#include <Rules.h>
#include <Schedule.h>
#include <solvers/BBSolver.h>
#include <solvers/DispatchSolver.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

std::filesystem::path instance_path;
Rules::Rule rule_to_use;
std::string rule_name;
std::string algorithm_name;

double timeout_seconds = std::numeric_limits<double>::infinity();

const std::map<std::string, Rules::Rule> available_rules = {
    {"shortest_processing_time", Rules::shortest_processing_time},
    {"most_work_remaining", Rules::most_work_remaining},
    {"first_come_first_served", Rules::first_come_first_served},
    {"random_operation", Rules::random_operation},
    {"longest_processing_time", Rules::longest_processing_time}};

const std::set<std::string> available_algorithms = {"priority_dispatch",
                                                    "branch_and_bound"};

// Function declarations
JobShopInstance instance_from_file(const std::filesystem::path &filepath);
void print_usage(const char *prog_name);
void read_args(int argc, char *argv[]);

void print_usage(const char *prog_name) {
  std::cout << "Usage: " << prog_name << " [options] <PATH_TO_INSTANCE>"
            << std::endl;
  std::cout << "Arguments:" << std::endl;
  std::cout << "\t<PATH_TO_INSTANCE>" << std::endl;
  std::cout
      << "\t\tPath to an instance file or a directory. If a directory "
         "is provided, it will be searched recursively for instance files."
      << std::endl;
  std::cout << "Options:" << std::endl;
  // ALGORITHM
  std::cout
      << "\t-a, --algorithm <ALGORITHM_NAME>" << std::endl
      << "\t\tSpecify the algorithm to use. Defaults to 'priority_dispatch'."
      << std::endl
      << std::endl;
  std::cout << "\t\tAvailable algorithms:" << std::endl;
  for (const auto &name : available_algorithms) {
    std::cout << "\t\t  - " << name << std::endl;
  }
  // RULE
  std::cout << "\t-r, --rule <RULE_NAME>" << std::endl
            << "\t\tSpecify the dispatch rule to use (only for "
               "priority_dispatch). Defaults to "
               "'shortest_processing_time'."
            << std::endl
            << std::endl;
  std::cout << "\t\tAvailable rules:" << std::endl;
  for (const auto &[name, rule] : available_rules) {
    std::cout << "\t\t  - " << name << std::endl;
  }
  // TIMEOUT
  std::cout << "\t-t, --timeout <SECONDS>" << std::endl
            << "\t\tSpecify a timeout in seconds. Defaults to no limit."
            << std::endl
            << std::endl;
  // HELP
  std::cout << "\t-h, --help" << std::endl
            << "\t\tDisplay this help message." << std::endl;
}

void read_args(int argc, char *argv[]) {
  // Defaults
  rule_name = "shortest_processing_time";
  rule_to_use = available_rules.at(rule_name);
  algorithm_name = "priority_dispatch";
  timeout_seconds = std::numeric_limits<double>::infinity();

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      print_usage(argv[0]);
      exit(0);
    } else if (arg == "-r" || arg == "--rule") {
      if (i + 1 < argc) {
        rule_name = argv[++i];
        if (available_rules.find(rule_name) == available_rules.end()) {
          std::cerr << "Error: Unknown rule '" << rule_name << "'."
                    << std::endl;
          print_usage(argv[0]);
          exit(1);
        }
        rule_to_use = available_rules.at(rule_name);
      } else {
        std::cerr << "Error: --rule option requires an argument." << std::endl;
        print_usage(argv[0]);
        exit(1);
      }
    } else if (arg == "-a" || arg == "--algorithm") {
      if (i + 1 < argc) {
        algorithm_name = argv[++i];
        if (available_algorithms.find(algorithm_name) ==
            available_algorithms.end()) {
          std::cerr << "Error: Unknown algorithm '" << algorithm_name << "'."
                    << std::endl;
          print_usage(argv[0]);
          exit(1);
        }
      } else {
        std::cerr << "Error: --algorithm option requires an argument."
                  << std::endl;
        print_usage(argv[0]);
        exit(1);
      }
    } else if (arg == "-t" || arg == "--timeout") {
      if (i + 1 < argc) {
        try {
          timeout_seconds = std::stod(argv[++i]);
          if (timeout_seconds <= 0) {
            std::cerr << "Error: Timeout must be a positive number."
                      << std::endl;
            exit(1);
          }
        } catch (const std::exception &e) {
          std::cerr << "Error: Invalid timeout value: " << e.what()
                    << std::endl;
          exit(1);
        }
      } else {
        std::cerr << "Error: --timeout option requires an argument."
                  << std::endl;
        print_usage(argv[0]);
        exit(1);
      }
    } else {
      instance_path = arg;
    }
  }

  if (instance_path.empty()) {
    std::cerr << "Error: Missing path to instance file or directory."
              << std::endl;
    print_usage(argv[0]);
    exit(1);
  }
}

int main(int argc, char *argv[]) {
  read_args(argc, argv);

  std::vector<std::filesystem::path> instance_files;

  if (!std::filesystem::exists(instance_path)) {
    std::cerr << "Error: Path does not exist: " << instance_path << std::endl;
    return 1;
  }

  if (std::filesystem::is_directory(instance_path)) {
    for (const auto &entry :
         std::filesystem::recursive_directory_iterator(instance_path)) {
      if (entry.is_regular_file()) {
        instance_files.push_back(entry.path());
      }
    }
  } else {
    instance_files.push_back(instance_path);
  }

  std::string output_filename =
      algorithm_name +
      (algorithm_name == "priority_dispatch" ? "_" + rule_name : "") + ".csv";
  std::ofstream output_file(output_filename);

  if (!output_file.is_open()) {
    std::cerr << "Error: Could not open output file " << output_filename
              << std::endl;
    return 1;
  }

  // Write CSV header
  output_file << "Instance,Runtime (ms),Makespan,TimedOut\n";
  std::cout << "Algorithm: " << algorithm_name << std::endl;
  if (algorithm_name == "priority_dispatch") {
    std::cout << "Rule: " << rule_name << std::endl;
  }
  if (timeout_seconds != std::numeric_limits<double>::infinity()) {
    std::cout << "Timeout: " << timeout_seconds << "s" << std::endl;
  }
  std::cout << std::string(70, '-') << std::endl;
  std::cout << std::left << std::setw(30) << "Instance" << std::setw(15)
            << "Runtime (ms)" << std::setw(15) << "Makespan" << std::setw(10)
            << "TimedOut" << std::endl;
  std::cout << std::string(70, '-') << std::endl;

  for (const auto &file_path : instance_files) {
    try {
      std::string instance_name = file_path.stem().string();
      std::cout << std::left << std::setw(30) << instance_name << std::flush;

      JobShopInstance instance = instance_from_file(file_path);
      int makespan = 0;

      auto start_time = std::chrono::steady_clock::now();

      std::chrono::steady_clock::time_point deadline; // Declare deadline

      if (timeout_seconds == std::numeric_limits<double>::infinity()) {
        deadline = std::chrono::steady_clock::time_point::max();
      } else {
        auto duration_to_add =
            std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                std::chrono::duration<double>(timeout_seconds));
        deadline = start_time + duration_to_add;
      }
      std::unique_ptr<ISolver> solver;

      if (algorithm_name == "priority_dispatch") {
        // Cria um solver de despacho
        solver = std::make_unique<DispatchSolver>(instance, rule_to_use);
      } else if (algorithm_name == "branch_and_bound") {
        // Cria um solver B&B
        solver = std::make_unique<BBSolver>(instance);
      }

      // Se o solver foi instanciado, resolve
      if (solver) {
        Schedule schedule = solver->solve(deadline);
        makespan = schedule.makespan();
      } else {
        throw std::runtime_error("Algoritmo não reconhecido.");
      }

      auto end_time = std::chrono::steady_clock::now();
      auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                          end_time - start_time)
                          .count();

      // --- Timeout Reporting ---
      bool timed_out = (end_time >= deadline);
      std::string timed_out_str = timed_out ? "Yes" : "No";

      std::cout << std::setw(15) << duration << std::setw(15) << makespan
                << std::setw(10) << timed_out_str << std::endl;
      output_file << instance_name << "," << duration << "," << makespan << ","
                  << timed_out_str << "\n";

    } catch (const std::exception &e) {
      std::cerr << "Error processing file " << file_path.string() << ": "
                << e.what() << std::endl;
    }
  }
  std::cout << std::string(70, '-') << std::endl;
  std::cout << "Processing complete. Results saved to " << output_filename
            << std::endl;
  output_file.close();

  return 0;
}

JobShopInstance instance_from_file(const std::filesystem::path &filepath) {
  std::ifstream file(filepath);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open file " + filepath.string());
  }

  int n_jobs, n_machines;
  file >> n_jobs >> n_machines;

  if (file.fail() || n_jobs <= 0 || n_machines <= 0) {
    throw std::runtime_error("Invalid header in file " + filepath.string());
  }

  std::vector<std::vector<Operation>> jobs(n_jobs,
                                           std::vector<Operation>(n_machines));
  for (int job_id = 0; job_id < n_jobs; ++job_id) {
    for (int op_idx = 0; op_idx < n_machines; ++op_idx) {
      int machine_id, duration;
      file >> machine_id >> duration;
      if (file.fail()) {
        throw std::runtime_error("Error reading operation data in " +
                                 filepath.string());
      }
      jobs[job_id][op_idx] = {
          job_id, machine_id, job_id * n_machines + op_idx, op_idx, duration,
      };
    }
  }

  return JobShopInstance(jobs, n_jobs, n_machines, n_jobs * n_machines);
}

```