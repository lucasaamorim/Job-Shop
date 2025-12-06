#include <solvers/SASolver.h>
#include <ScheduleState.h>

#include <algorithm>
#include <cmath>

Schedule SASolver::solve(std::chrono::steady_clock::time_point deadline) {
  // 1. Initialization
  Chromosome current_chrom = generate_initial_chrom();
  Schedule current_schedule = decode(current_chrom);
  int current_makespan = current_schedule.makespan();

  // Track the global best found so far
  Schedule best_schedule = current_schedule;
  int best_makespan = current_makespan;

  // 2. Annealing Loop
  while (std::chrono::steady_clock::now() < deadline && temp > epsilon) {
    // Generate neighbor (copy then mutate)
    Chromosome neighbor_chrom = current_chrom;
    get_neighbor(neighbor_chrom);

    // Evaluate neighbor
    Schedule neighbor_schedule = decode(neighbor_chrom);
    int neighbor_makespan = neighbor_schedule.makespan();

    // Calculate Delta E (Change in energy/makespan)
    int delta = neighbor_makespan - current_makespan;

    // 3. Acceptance Criteria
    // If better (delta < 0) OR probability check passes
    if (should_accept(delta, temp)) {
      current_chrom = neighbor_chrom;
      current_makespan = neighbor_makespan;
      current_schedule = neighbor_schedule;

      // Update global best if this is the best we've ever seen
      if (current_makespan < best_makespan) {
        best_makespan = current_makespan;
        best_schedule = current_schedule;
      }
    }

    // 4. Cool down
    temp *= cooling_rate;
  }

  return best_schedule;
}

Schedule SASolver::decode(const SASolver::Chromosome &chrom) {
  // Leverage existing logic: ScheduleState manages machine availability
  // and operation precedence automatically.
  ScheduleState state(instance);

  for (int job_id : chrom) {
    // The chromosome says "Schedule Job X next".
    // We ask the state which specific operation that is.
    int op_index = state.job_next_operation[job_id];

    // Safety check: ensure the job actually has ops left
    if (op_index < instance.jobs[job_id].size()) {
      Operation op = instance.jobs[job_id][op_index];
      state.dispatch(op);
    }
  }
  return state.schedule;
}

SASolver::Chromosome SASolver::generate_initial_chrom() {
  Chromosome chrom;
  chrom.reserve(instance.n_operations);

  // Fill chromosome with Job IDs: [0, 0, ..., 1, 1, ...]
  for (int j = 0; j < instance.n_jobs; ++j) {
    // Each job appears in the list as many times as it has operations
    size_t num_ops = instance.jobs[j].size();
    for (size_t k = 0; k < num_ops; ++k) {
      chrom.push_back(j);
    }
  }

  // Shuffle to create a random valid permutation
  std::shuffle(chrom.begin(), chrom.end(), rng);
  return chrom;
}

void SASolver::get_neighbor(SASolver::Chromosome &chrom) {
  if (chrom.size() < 2) return;

  // Simple swap mutation
  std::uniform_int_distribution<> dist(0, chrom.size() - 1);
  int i = dist(rng);
  int j = dist(rng);

  // Ensure we swap distinct indices
  while (i == j) {
    j = dist(rng);
  }

  std::swap(chrom[i], chrom[j]);
}

bool SASolver::should_accept(int delta, double current_temp) {
  // If the new solution is better (delta < 0), always accept.
  if (delta < 0) {
    return true;
  }

  // If worse, accept with probability e^(-delta / T)
  std::uniform_real_distribution<> dist(0.0, 1.0);
  double probability = std::exp(-delta / current_temp);

  return dist(rng) < probability;
}
