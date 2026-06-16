/**
 *
 * @file BenchmarkJson.hpp
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

#include "common/Benchmark.hpp"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <vix/json/json.hpp>

namespace vix::bench
{
  using Json = vix::json::Json;

  inline std::string compiler_name()
  {
#if defined(__clang__)
    return "clang";
#elif defined(__GNUC__)
    return "gcc";
#elif defined(_MSC_VER)
    return "msvc";
#else
    return "unknown";
#endif
  }

  inline std::string compiler_version()
  {
#if defined(__clang__)
    return std::to_string(__clang_major__) + "." +
           std::to_string(__clang_minor__) + "." +
           std::to_string(__clang_patchlevel__);
#elif defined(__GNUC__)
    return std::to_string(__GNUC__) + "." +
           std::to_string(__GNUC_MINOR__) + "." +
           std::to_string(__GNUC_PATCHLEVEL__);
#elif defined(_MSC_VER)
    return std::to_string(_MSC_VER);
#else
    return "unknown";
#endif
  }

  inline std::string operating_system()
  {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#elif defined(__FreeBSD__)
    return "freebsd";
#else
    return "unknown";
#endif
  }

  inline std::string architecture()
  {
#if defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
  }

  inline std::string build_type()
  {
#if defined(NDEBUG)
    return "Release";
#else
    return "Debug";
#endif
  }

  inline std::string env_or_default(
      const char *name,
      const std::string &fallback = "")
  {
    const char *value = std::getenv(name);

    if (value == nullptr)
    {
      return fallback;
    }

    return value;
  }

  inline std::string utc_timestamp_hint()
  {
    const auto now =
        std::chrono::system_clock::now().time_since_epoch();

    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(now).count();

    return std::to_string(seconds);
  }

  inline Json sample_to_json(const BenchmarkSample &sample)
  {
    return Json{
        {"elapsed_ms", sample.elapsed_ms},
        {"ops_per_sec", sample.ops_per_sec},
    };
  }

  inline Json result_to_json(const BenchmarkResult &result)
  {
    Json samples = Json::array();

    for (const BenchmarkSample &sample : result.samples)
    {
      samples.push_back(sample_to_json(sample));
    }

    return Json{
        {"name", result.name},
        {"operations", result.operations},

        {"warmup_iterations", result.warmup_iterations},
        {"measure_iterations", result.measure_iterations},

        {"min_ms", result.min_ms},
        {"max_ms", result.max_ms},
        {"mean_ms", result.mean_ms},
        {"median_ms", result.median_ms},

        {"min_ops_per_sec", result.min_ops_per_sec},
        {"max_ops_per_sec", result.max_ops_per_sec},
        {"mean_ops_per_sec", result.mean_ops_per_sec},
        {"median_ops_per_sec", result.median_ops_per_sec},

        {"samples", samples},
    };
  }

  inline Json results_to_json(const std::vector<BenchmarkResult> &results)
  {
    Json output = Json::array();

    for (const BenchmarkResult &result : results)
    {
      output.push_back(result_to_json(result));
    }

    return output;
  }

  inline Json make_report_json(
      const std::string &suite,
      const std::string &version,
      const std::vector<BenchmarkResult> &results)
  {
    return Json{
        {"suite", suite},
        {"version", version},

        {"timestamp_unix", utc_timestamp_hint()},

        {"machine", env_or_default("VIX_BENCH_MACHINE", "local")},
        {"runner", env_or_default("VIX_BENCH_RUNNER", "manual")},

        {"os", operating_system()},
        {"arch", architecture()},
        {"build_type", build_type()},

        {"compiler",
         {
             {"name", compiler_name()},
             {"version", compiler_version()},
         }},

        {"results", results_to_json(results)},
    };
  }

  inline void write_json_file(
      const std::filesystem::path &path,
      const Json &json)
  {
    std::error_code ec;

    const std::filesystem::path parent = path.parent_path();

    if (!parent.empty())
    {
      std::filesystem::create_directories(parent, ec);
    }

    std::ofstream out(path, std::ios::binary | std::ios::trunc);

    if (!out)
    {
      throw std::runtime_error(
          "failed to open benchmark json output: " + path.string());
    }

    out << json.dump(2);
    out << '\n';
  }

  inline void write_report_json(
      const std::filesystem::path &path,
      const std::string &suite,
      const std::string &version,
      const std::vector<BenchmarkResult> &results)
  {
    write_json_file(
        path,
        make_report_json(
            suite,
            version,
            results));
  }

} // namespace vix::bench
