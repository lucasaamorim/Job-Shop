#pragma once

#include <Dispatcher.h>
#include <Operation.h>

#include <algorithm>
#include <functional>
#include <random>

namespace Rules {
typedef std::function<Operation(Dispatcher &)> Rule;
inline const Rule shortest_processing_time =
    [](Dispatcher &dispatcher) -> Operation {
  auto candidates = dispatcher.available_operations();
  return *std::min_element(candidates.begin(), candidates.end(),
                           [](const Operation &a, const Operation &b) {
                             return a.duration < b.duration;
                           });
};

inline const Rule most_work_remaining =
    [](Dispatcher &dispatcher) -> Operation {
  auto candidates = dispatcher.available_operations();
  return *std::max_element(
      candidates.begin(), candidates.end(),
      [&dispatcher](const Operation &a, const Operation &b) {
        return dispatcher.job_work_remaining[a.job_id] <
               dispatcher.job_work_remaining[b.job_id];
      }

  );
};

inline const Rule first_come_first_served =
    [](Dispatcher &dispatcher) -> Operation {
  auto candidates = dispatcher.available_operations();
  return *min_element(candidates.begin(), candidates.end(),
                      [](const Operation &a, const Operation &b) {
                        return a.position_in_job < b.position_in_job;
                      });
};

inline const Rule random_operation = [](Dispatcher &dispatcher) -> Operation {
  auto candidates = dispatcher.available_operations();
  std::random_device rd;
  std::mt19937 gen(rd());

  std::uniform_int_distribution<> distrib(0, candidates.size()-1);

  return candidates[distrib(gen)];
};

inline const Rule longest_processing_time =
    [](Dispatcher &dispatcher) -> Operation {
  auto candidates = dispatcher.available_operations();
  return *std::max_element(candidates.begin(), candidates.end(),
                           [](const Operation &a, const Operation &b) {
                             return a.duration < b.duration;
                           });
};
} // namespace Rules
