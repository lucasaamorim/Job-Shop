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
