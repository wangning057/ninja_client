// Copyright 2019 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef NINJA_PARALLEL_MAP_H_
#define NINJA_PARALLEL_MAP_H_

#include <functional>
#include <mutex>
#include <utility>
#include <vector>

#include "thread_pool.h"
#include "util.h"

namespace detail {

template <typename T>
struct MapFnResults {
  MapFnResults(size_t size) : size_(size), results_(new T[size] {}) {}

  using ReturnType = std::vector<T>;

  template <typename Vector, typename MapFn>
  void MapItem(Vector& vec, const MapFn& map_fn, size_t idx) {
    assert(idx < size_);
    results_.get()[idx] = map_fn(vec[idx]);
  }

  ReturnType Finish() {
    ReturnType result;
    result.reserve(size_);
    std::move(results_.get(), results_.get() + size_,
              std::back_inserter(result));
    return result;
  }

private:
  size_t size_;

  /// We'd like to store the intermediate results using std::vector<T>, but
  /// the std::vector<bool> specialization doesn't allow concurrent modification
  /// of different elements, so use a simple array instead.
  std::unique_ptr<T[]> results_;
};

template <>
struct MapFnResults<void> {
  MapFnResults(size_t) {}

  using ReturnType = void;

  template <typename Vector, typename MapFn>
  void MapItem(Vector& vec, const MapFn& map_fn, size_t idx) {
    map_fn(vec[idx]);
  }

  ReturnType Finish() {}
};

} // namespace detail

template <typename T>
std::vector<std::pair<T, T>> SplitByThreads(T total) {
  return SplitByCount(total, GetOptimalThreadPoolJobCount());
}

/// Run |map_fn| on each element of |vec| and return a vector containing the
/// result, in the same order as the input vector. Returns void instead if the
/// map function returns void.
///
/// The map function is invoked from multiple threads, so the functor is marked
/// const. (e.g. A "mutable" lambda isn't allowed.)
template <typename Vector, typename MapFn>
auto ParallelMap(ThreadPool* thread_pool, Vector&& vec, const MapFn& map_fn) ->
    typename detail::MapFnResults<
      typename std::remove_reference<
        decltype(map_fn(vec[0]))>::type>::ReturnType {
  // Identify the return type of the map function, and if it isn't void, prepare
  // a vector to hold the result of mapping each item in the sequence.
  using MapReturn = typename std::remove_reference<decltype(map_fn(vec[0]))>::type;
  detail::MapFnResults<MapReturn> results(vec.size());

  // Split the sequence up into groups.
  std::vector<std::pair<size_t, size_t>> ranges = SplitByThreads(vec.size());
  std::vector<std::function<void()>> tasks;
  for (auto& range : ranges) {
    tasks.emplace_back([&results, &vec, &map_fn, range] {
      for (size_t idx = range.first; idx < range.second; ++idx) {
        results.MapItem(vec, map_fn, idx);
      }
    });
  }
  thread_pool->RunTasks(std::move(tasks));

  return results.Finish();
}

#endif  // NINJA_PARALLEL_MAP_H_
