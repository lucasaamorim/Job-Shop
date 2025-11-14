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
