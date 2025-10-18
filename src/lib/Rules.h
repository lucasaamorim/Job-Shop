#pragma once

#include <Operation.h>

#include <algorithm>
#include <functional>
#include <random>

namespace Rules {
typedef std::function<bool(const Operation &, const Operation &)> Rule;
inline const Rule shortest_processing_time = [](const Operation &a,
                                                const Operation &b) -> bool {
  return a.duration < b.duration;
};

//TODO: Find out how to handle this
inline const Rule most_work_remaining = [](const Operation &a,
                                           const Operation &b) -> bool {
  auto candidates = dispatcher.available_operations();
  return *std::max_element(
      candidates.begin(), candidates.end(),
      [&dispatcher](const Operation &a, const Operation &b) {
        return dispatcher.job_work_remaining[a.job_id] <
               dispatcher.job_work_remaining[b.job_id];
      }

  );
};

inline const Rule first_come_first_served = [](const Operation &a,
                                               const Operation &b) -> bool {
  return a.position_in_job < b.position_in_job;
};

//TODO: Find out how to handle this
inline const Rule random_operation = [](const Operation &a,
                                        const Operation &b) -> bool {
  auto candidates = dispatcher.available_operations();
  std::random_device rd;
  std::mt19937 gen(rd());

  std::uniform_int_distribution<> distrib(0, candidates.size() - 1);

  return candidates[distrib(gen)];
};

inline const Rule longest_processing_time = [](const Operation &a,
                                               const Operation &b) -> bool {
  return a.duration > b.duration;
};
} // namespace Rules
