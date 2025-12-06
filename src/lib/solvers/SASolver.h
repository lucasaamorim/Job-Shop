#pragma once

#include <ISolver.h>
#include <chrono>
#include <random>
#include <vector>

struct SASolver : public ISolver {
  /* The Chromossome will be just a way of encoding the current state, where it
   * will have a permutation of lenght n*m containing m occurrences of the
   * numbers from 0 to n-1, so, for a simple instance with 2 jobs and two
   * machines, we could have a chromossome like: {0, 1, 0, 1} And that would
   * mean that our solution is obtained by:
   * 1. Scheduling the first operation from job 0
   * 2. Scheduling the first operation from job 1
   * 3. Shcheduling the second operation from job 0
   * 4. Scheduling the second operation from job 1
   * Because the way the encoding of the solution works is:
   * On the i-th position that has a value j in it, schedule the next available
   * operation from job j. That means that ANY and ALL valid solutions can be
   * represented in this way. With the caveat that some permutations imply on
   * the exact same schedule as others.
   * */
  typedef std::vector<int> Chromosome;

  double temp;
  double cooling_rate;
  Chromosome cur_chrom;
  double epsilon = 1e-4;

  std::mt19937 rng;

  SASolver(const JobShopInstance instance, double initial_temp = 1000,
           double cooling = 0.995)
      : ISolver(instance), temp(initial_temp), cooling_rate(cooling),
        rng(std::random_device()()) {}

  Schedule solve(std::chrono::steady_clock::time_point deadline) override;

  Schedule decode(const Chromosome &chrom);

  Chromosome generate_initial_chrom();

  void get_neighbor(Chromosome &chrom);

  bool should_accept(int delta, double temp);
};
