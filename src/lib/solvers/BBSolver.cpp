#include <solvers/BBSolver.h>
#include <solvers/DispatchSolver.h>

#include <Rules.h>
#include <algorithm>
#include <climits>
#include <memory>
#include <optional>
#include <queue>
#include <vector>

namespace {

/**
 * @brief Represents a node in the Branch and Bound search tree.
 */
struct Node {
  std::vector<int> next_op_index;     //!< Index of the next op for each job
  std::vector<int> machine_free_time; //!< Earliest time each machine is free
  std::vector<int>
      job_completion_time; //!< Time the last op of each job finished
  int current_makespan;    //!< Max completion time of scheduled ops
  int lower_bound;         //!< The lower bound (max of all job/machine LBs)

  // Store individual LB components for incremental updates
  std::vector<int> lb_per_job;
  std::vector<int> lb_per_machine;

  std::shared_ptr<const Node> parent; //!< Pointer to parent for backtracking
  std::optional<ScheduledOperation>
      scheduled_op; //!< The op that led to this node

  bool operator>(const Node &other) const {
    return lower_bound > other.lower_bound;
  }
};

/**
 * @brief Comparator for the priority queue (Best-First Search).
 */
struct NodeComparator {
  bool operator()(const std::shared_ptr<const Node> &a,
                  const std::shared_ptr<const Node> &b) const {
    // We want a min-heap based on lower_bound
    return a->lower_bound > b->lower_bound;
  }
};

// --- 1|rj|Cmax Lower Bound Helpers ---

/**
 * @brief Data structure for a single machine scheduling problem.
 */
struct OperationDataToSingleMachine {
  int job_id;
  int duration;
  int release_time;
};

/**
 * @brief Comparator for Schrage's algorithm (min-heap on release time).
 */
struct ReleaseTimeComparator {
  bool operator()(const OperationDataToSingleMachine &a,
                  const OperationDataToSingleMachine &b) const {
    return a.release_time > b.release_time;
  }
};

/**
 * @brief Comparator for Schrage's algorithm (max-heap on processing time).
 */
struct ProcessingTimeComparator {
  bool operator()(const OperationDataToSingleMachine &a,
                  const OperationDataToSingleMachine &b) const {
    if (a.duration != b.duration)
      return a.duration < b.duration;       // Max-heap on duration
    return a.release_time > b.release_time; // Tie-breaker
  }
};

/**
 * @brief Calculates the earliest start time (EST) for a given operation
 * based on job and machine readiness.
 */
int calculate_earliest_start_time(const Node &node, const Operation &op) {
  // Operation cannot start before its job's previous op is done
  int job_ready =
      (op.position_in_job > 0) ? node.job_completion_time[op.job_id] : 0;
  // Operation cannot start before its machine is free
  int machine_ready = node.machine_free_time[op.machine_id];

  return std::max(job_ready, machine_ready);
}

/**
 * @brief Solves the 1|rj|Cmax single-machine problem.
 * This is used to calculate a lower bound for a single machine.
 */
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
    // Move all operations released by current_time to the ready queue
    while (!not_released.empty() &&
           not_released.top().release_time <= current_time) {
      ready.push(not_released.top());
      not_released.pop();
    }

    if (!ready.empty()) {
      // Schedule the next operation from the ready queue (SPT)
      auto op = ready.top();
      ready.pop();
      int start = std::max(current_time, op.release_time);
      current_time = start + op.duration;
      max_completion = std::max(max_completion, current_time);
    } else if (!not_released.empty()) {
      // No ops are ready; jump time forward to the next release
      current_time = not_released.top().release_time;
    }
  }
  return max_completion;
}

/**
 * @brief Computes the lower bound contribution for a single machine.
 */
int compute_machine_lb_component(const Node &node,
                                 const JobShopInstance &instance,
                                 int machine_id) {
  std::vector<OperationDataToSingleMachine> ops;
  // We must scan all jobs to find all future operations for this machine.
  for (int j = 0; j < instance.n_jobs; ++j) {
    int next_idx = node.next_op_index[j];

    for (size_t k = next_idx; k < instance.jobs[j].size(); ++k) {
      const auto &op = instance.jobs[j][k];
      if (op.machine_id == machine_id) {
        // Calculate the release time for this operation.
        // It's the job's completion time + durations of all intermediate
        // operations.
        int r_j = node.job_completion_time[j];
        for (size_t p = next_idx; p < k; ++p) {
          r_j += instance.jobs[j][p].duration;
        }

        // The operation also can't start before the machine is free
        r_j = std::max(r_j, node.machine_free_time[machine_id]);

        ops.push_back({op.job_id, op.duration, r_j});
      }
    }
  }
  return solve_1_rj_cmax(ops);
}

/**
 * @brief Computes the lower bound contribution for a single job.
 * (This is just the job's current completion time + all remaining work).
 */
int compute_job_lb_component(const Node &node, const JobShopInstance &instance,
                             int job_id) {
  int remaining_work = 0;
  for (size_t k = node.next_op_index[job_id]; k < instance.jobs[job_id].size();
       ++k) {
    remaining_work += instance.jobs[job_id][k].duration;
  }
  return node.job_completion_time[job_id] + remaining_work;
}

/**
 * @brief Performs a full calculation of all LB components.
 * Used for the root node.
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
 * @brief Incrementally updates the lower bound after scheduling an operation.
 * This is much faster than re-computing all components.
 */
void update_lower_bound_incremental(Node &node, const JobShopInstance &instance,
                                    const Operation &scheduled_op) {
  int max_lb = node.current_makespan;

  // 1. Update the LB for the job that was just scheduled
  node.lb_per_job[scheduled_op.job_id] =
      compute_job_lb_component(node, instance, scheduled_op.job_id);

  // 2. Update the LB for the machine that was just used
  node.lb_per_machine[scheduled_op.machine_id] =
      compute_machine_lb_component(node, instance, scheduled_op.machine_id);

  // 3. Rigorous update: The job's completion time changed, which means
  //    the release times for its future operations on *other* machines
  //    have also changed. We must recompute those machine LBs.
  for (size_t k = node.next_op_index[scheduled_op.job_id];
       k < instance.jobs[scheduled_op.job_id].size(); ++k) {
    int m_id = instance.jobs[scheduled_op.job_id][k].machine_id;
    if (m_id != scheduled_op.machine_id) {
      node.lb_per_machine[m_id] =
          compute_machine_lb_component(node, instance, m_id);
    }
  }

  // 4. Aggregate to find the new max lower bound
  for (int lb : node.lb_per_job)
    max_lb = std::max(max_lb, lb);
  for (int lb : node.lb_per_machine)
    max_lb = std::max(max_lb, lb);

  node.lower_bound = max_lb;
}

} // namespace

// --- BBSolver Implementation ---

/**
 * @brief Constructor: Initializes the solver and finds an initial
 * upper bound by running several fast heuristics.
 */
BBSolver::BBSolver(const JobShopInstance &instance)
    : ISolver(instance), best_schedule(Schedule(instance)) {
  // Run a few dispatch rules to get a good initial upper bound.
  // A tighter initial bound leads to more pruning.
  std::vector<Schedule> heuristic_schedules;
  heuristic_schedules.push_back(
      DispatchSolver(instance, Rules::shortest_processing_time)
          .solve(std::chrono::steady_clock::time_point::max()));
  heuristic_schedules.push_back(
      DispatchSolver(instance, Rules::most_work_remaining)
          .solve(std::chrono::steady_clock::time_point::max()));
  heuristic_schedules.push_back(
      DispatchSolver(instance, Rules::first_come_first_served)
          .solve(std::chrono::steady_clock::time_point::max()));
  heuristic_schedules.push_back(
      DispatchSolver(instance, Rules::longest_processing_time)
          .solve(std::chrono::steady_clock::time_point::max()));

  // Find the best schedule among the heuristics
  auto best_it =
      std::min_element(heuristic_schedules.begin(), heuristic_schedules.end(),
                       [](const Schedule &a, const Schedule &b) {
                         return a.makespan() < b.makespan();
                       });

  best_schedule = *best_it;
  upper_bound = best_schedule.makespan();
}

Schedule BBSolver::solve(std::chrono::steady_clock::time_point deadline) {
  // Priority queue for Best-First Search (nodes with lower LB are explored
  // first)
  std::priority_queue<std::shared_ptr<const Node>,
                      std::vector<std::shared_ptr<const Node>>, NodeComparator>
      queue;

  // 1. Initialize root node
  auto root_node = std::make_shared<Node>();
  root_node->next_op_index.assign(instance.n_jobs, 0);
  root_node->machine_free_time.assign(instance.n_machines, 0);
  root_node->job_completion_time.assign(instance.n_jobs, 0);
  root_node->current_makespan = 0;
  initialize_lower_bound(*root_node, instance); // Full LB calculation
  queue.push(root_node);

  while (!queue.empty()) {
    // 2. Check for timeout
    if (std::chrono::steady_clock::now() > deadline) {
      break; // Time limit reached, return the best solution found so far
    }

    // 3. Pop the most promising node (lowest lower_bound)
    auto current_node_ptr = queue.top();
    queue.pop();

    // 4. Pruning (Fathoming)
    // If this node's LB is already worse than our best-known solution,
    // discard it and all its children.
    if (current_node_ptr->lower_bound >= upper_bound)
      continue;

    // 5. Goal Check
    // Are all operations scheduled?
    bool all_jobs_finished = true;
    for (int job = 0; job < instance.n_jobs; job++) {
      if (current_node_ptr->next_op_index[job] < instance.jobs[job].size()) {
        all_jobs_finished = false;
        break;
      }
    }

    if (all_jobs_finished) {
      // Found a complete schedule. Is it better than our current best?
      if (current_node_ptr->current_makespan < upper_bound) {
        upper_bound = current_node_ptr->current_makespan;

        // Reconstruct the best schedule by tracing parent pointers
        Schedule new_best_schedule(instance);
        auto trace_ptr = current_node_ptr;
        std::vector<ScheduledOperation> ops;
        while (trace_ptr->parent) {
          ops.push_back(trace_ptr->scheduled_op.value());
          trace_ptr = trace_ptr->parent;
        }
        std::reverse(ops.begin(), ops.end());
        for (const auto &op : ops)
          new_best_schedule.add(op);
        best_schedule = new_best_schedule;
      }
      continue; // This leaf node is processed
    }

    // 6. Branching
    // Identify the "critical machine" and branch on its operations.
    // This is a common, effective branching strategy.

    // Get the set of all available "next" operations
    std::vector<Operation> eligible_ops;
    for (int job = 0; job < instance.n_jobs; ++job) {
      if (current_node_ptr->next_op_index[job] < instance.jobs[job].size()) {
        eligible_ops.push_back(
            instance.jobs[job][current_node_ptr->next_op_index[job]]);
      }
    }

    // Find the "critical machine": the one that can complete an op the
    // earliest.
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

    // Branch: Create a new child node for each eligible operation
    // *on that critical machine*.
    for (const auto &op_to_schedule : eligible_ops) {
      if (op_to_schedule.machine_id == critical_machine_id) {

        auto new_node = std::make_shared<Node>(*current_node_ptr);
        new_node->parent = current_node_ptr;

        // Update the state for the new child node
        int start_time =
            std::max(new_node->machine_free_time[op_to_schedule.machine_id],
                     new_node->job_completion_time[op_to_schedule.job_id]);
        int completion_time = start_time + op_to_schedule.duration;

        new_node->machine_free_time[op_to_schedule.machine_id] =
            completion_time;
        new_node->job_completion_time[op_to_schedule.job_id] = completion_time;
        new_node->current_makespan =
            std::max(new_node->current_makespan, completion_time);
        new_node->next_op_index[op_to_schedule.job_id]++;
        new_node->scheduled_op =
            ScheduledOperation(op_to_schedule, start_time, completion_time);

        // Calculate the lower bound for this new child
        update_lower_bound_incremental(*new_node, instance, op_to_schedule);

        // Add the child to the queue if it's not pruned
        if (new_node->lower_bound < upper_bound) {
          queue.push(new_node);
        }
      }
    }
  }

  // Loop finished (either by exhaustion or timeout)
  return best_schedule;
}
