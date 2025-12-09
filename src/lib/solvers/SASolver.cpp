#include <ScheduleState.h>
#include <solvers/SASolver.h>

#include <algorithm>
#include <cmath>
#include <iostream>

Schedule SASolver::solve(std::chrono::steady_clock::time_point deadline) {
  auto_tune_parameters(deadline);
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

void SASolver::auto_tune_parameters(
    std::chrono::steady_clock::time_point deadline) {
  // 1. Measure Average Degradation (Warm-up)
  // ---------------------------------------------------------
  const int warm_up_iters = 100;
  long long total_degradation = 0;
  int degradation_count = 0;

  // Create a temporary state for the random walk
  Chromosome temp_chrom = generate_initial_chrom();
  int current_makespan = decode(temp_chrom).makespan();

  auto start_time = std::chrono::steady_clock::now();

  for (int i = 0; i < warm_up_iters; ++i) {
    Chromosome next_chrom = temp_chrom;
    get_neighbor(next_chrom);
    int next_makespan = decode(next_chrom).makespan();

    if (next_makespan > current_makespan) {
      total_degradation += (next_makespan - current_makespan);
      degradation_count++;
    }
    // In warm-up, we always accept to explore freely
    current_makespan = next_makespan;
    temp_chrom = next_chrom;
  }

  auto end_time = std::chrono::steady_clock::now();

  // 2. Calculate Initial Temperature (Kirkpatrick's Method)
  // ---------------------------------------------------------
  double avg_degradation = degradation_count > 0
                               ? (double)total_degradation / degradation_count
                               : 1.0;

  // Target acceptance of 80% at start
  this->temp = -avg_degradation / std::log(0.8);

  // 3. Calculate Stopping Epsilon
  // ---------------------------------------------------------
  // We want the probability of accepting a degradation of 1 unit
  // to be essentially zero (e.g., 1e-5) at the end.
  double target_end_prob = 1e-5;
  this->epsilon = -1.0 / std::log(target_end_prob);

  // 4. Calculate Dynamic Cooling Rate based on Time
  // ---------------------------------------------------------
  auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         end_time - start_time)
                         .count();
  double ms_per_iter = (double)duration_ms / warm_up_iters;

  auto remaining_time = deadline - std::chrono::steady_clock::now();
  double remaining_ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(remaining_time)
          .count();

  remaining_ms *= 0.95;

  long expected_iterations =
      (ms_per_iter > 0) ? (long)(remaining_ms / ms_per_iter) : 1000;

  // Formula: alpha = (epsilon / T0) ^ (1 / N)
  if (expected_iterations > 0) {
    this->cooling_rate =
        std::pow(this->epsilon / this->temp, 1.0 / expected_iterations);
  }

  // std::cout << "[AutoTune] T_0: " << this->temp
  //           << " | Epsilon: " << this->epsilon
  //           << " | Alpha: " << this->cooling_rate
  //           << " | Est. Iters: " << expected_iterations << std::endl;
}

void SASolver::get_neighbor(SASolver::Chromosome &chrom) {
  if (chrom.size() < 2)
    return;

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
