/**
 *
 * @file Benchmark.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#pragma once

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace vix::bench
{
  using Clock = std::chrono::steady_clock;
  using Nanoseconds = std::chrono::nanoseconds;

  struct BenchmarkConfig
  {
    std::uint32_t warmup_iterations{3};
    std::uint32_t measure_iterations{15};
  };

  struct BenchmarkSample
  {
    double elapsed_ms{0.0};
    double ops_per_sec{0.0};
  };

  struct BenchmarkResult
  {
    std::string name{};
    std::uint64_t operations{0};

    std::uint32_t warmup_iterations{0};
    std::uint32_t measure_iterations{0};

    double min_ms{0.0};
    double max_ms{0.0};
    double mean_ms{0.0};
    double median_ms{0.0};

    double min_ops_per_sec{0.0};
    double max_ops_per_sec{0.0};
    double mean_ops_per_sec{0.0};
    double median_ops_per_sec{0.0};

    std::vector<BenchmarkSample> samples{};
  };

  template <class T>
  inline void do_not_optimize(const T &value)
  {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : "g"(value) : "memory");
#elif defined(_MSC_VER)
    (void)value;
    _ReadWriteBarrier();
#else
    (void)value;
#endif
  }

  inline void clobber_memory()
  {
#if defined(__GNUC__) || defined(__clang__)
    asm volatile("" : : : "memory");
#elif defined(_MSC_VER)
    _ReadWriteBarrier();
#endif
  }

  inline double to_ms(Nanoseconds duration)
  {
    return static_cast<double>(duration.count()) / 1'000'000.0;
  }

  inline double ops_per_sec(
      std::uint64_t operations,
      double elapsed_ms)
  {
    if (operations == 0 || elapsed_ms <= 0.0)
    {
      return 0.0;
    }

    return static_cast<double>(operations) * 1000.0 / elapsed_ms;
  }

  inline double median(std::vector<double> values)
  {
    if (values.empty())
    {
      return 0.0;
    }

    std::sort(values.begin(), values.end());

    const std::size_t mid = values.size() / 2u;

    if ((values.size() % 2u) == 0u)
    {
      return (values[mid - 1u] + values[mid]) / 2.0;
    }

    return values[mid];
  }

  inline double mean(const std::vector<double> &values)
  {
    if (values.empty())
    {
      return 0.0;
    }

    const double total =
        std::accumulate(values.begin(), values.end(), 0.0);

    return total / static_cast<double>(values.size());
  }

  template <class Fn>
  BenchmarkSample measure_once(
      std::uint64_t operations,
      Fn &&fn)
  {
    const auto start = Clock::now();

    std::forward<Fn>(fn)();

    const auto end = Clock::now();

    const double elapsed_ms =
        to_ms(std::chrono::duration_cast<Nanoseconds>(end - start));

    return BenchmarkSample{
        .elapsed_ms = elapsed_ms,
        .ops_per_sec = ops_per_sec(operations, elapsed_ms),
    };
  }

  template <class Fn>
  BenchmarkResult run(
      std::string name,
      std::uint64_t operations,
      Fn &&fn,
      BenchmarkConfig config = {})
  {
    for (std::uint32_t i = 0; i < config.warmup_iterations; ++i)
    {
      std::forward<Fn>(fn)();
      clobber_memory();
    }

    BenchmarkResult result;

    result.name = std::move(name);
    result.operations = operations;
    result.warmup_iterations = config.warmup_iterations;
    result.measure_iterations = config.measure_iterations;

    result.samples.reserve(config.measure_iterations);

    for (std::uint32_t i = 0; i < config.measure_iterations; ++i)
    {
      BenchmarkSample sample =
          measure_once(
              operations,
              fn);

      result.samples.push_back(sample);

      clobber_memory();
    }

    std::vector<double> elapsed_values;
    std::vector<double> ops_values;

    elapsed_values.reserve(result.samples.size());
    ops_values.reserve(result.samples.size());

    for (const BenchmarkSample &sample : result.samples)
    {
      elapsed_values.push_back(sample.elapsed_ms);
      ops_values.push_back(sample.ops_per_sec);
    }

    if (!elapsed_values.empty())
    {
      result.min_ms =
          *std::min_element(elapsed_values.begin(), elapsed_values.end());

      result.max_ms =
          *std::max_element(elapsed_values.begin(), elapsed_values.end());

      result.mean_ms = mean(elapsed_values);
      result.median_ms = median(elapsed_values);
    }

    if (!ops_values.empty())
    {
      result.min_ops_per_sec =
          *std::min_element(ops_values.begin(), ops_values.end());

      result.max_ops_per_sec =
          *std::max_element(ops_values.begin(), ops_values.end());

      result.mean_ops_per_sec = mean(ops_values);
      result.median_ops_per_sec = median(ops_values);
    }

    return result;
  }

  inline void print_result(const BenchmarkResult &result)
  {
    std::cout
        << std::left << std::setw(36) << result.name
        << " | ops=" << std::setw(12) << result.operations
        << " | median_ms=" << std::setw(12) << std::fixed << std::setprecision(3) << result.median_ms
        << " | mean_ms=" << std::setw(12) << std::fixed << std::setprecision(3) << result.mean_ms
        << " | ops/sec=" << std::fixed << std::setprecision(2) << result.median_ops_per_sec
        << '\n';
  }

  inline void print_results(const std::vector<BenchmarkResult> &results)
  {
    for (const BenchmarkResult &result : results)
    {
      print_result(result);
    }
  }

} // namespace vix::bench
