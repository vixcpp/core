/**
 *
 * @file ThreadPoolExecutor.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_THREAD_POOL_EXECUTOR_HPP
#define VIX_THREAD_POOL_EXECUTOR_HPP

#include <memory>
#include <functional>

#include <vix/executor/IExecutor.hpp>
#include <vix/threadpool/ThreadPool.hpp>
#include <vix/executor/TaskOptions.hpp>
#include <vix/executor/Metrics.hpp>

namespace vix::experimental
{

  /**
   * @brief IExecutor implementation backed by vix::threadpool::ThreadPool.
   */
  class ThreadPoolExecutor final : public vix::executor::IExecutor
  {
  public:
    /**
     * @brief Create a thread pool executor.
     *
     * @param threads Initial worker thread count.
     * @param maxThreads Maximum worker thread count.
     * @param defaultPriority Default task priority.
     */
    explicit ThreadPoolExecutor(
        std::size_t threads,
        std::size_t maxThreads,
        int defaultPriority);

    /** @brief Post a task for execution. */
    bool post(std::function<void()> fn, vix::executor::TaskOptions opt = {}) override;

    /** @brief Return executor metrics snapshot. */
    vix::executor::Metrics metrics() const override;

    /** @brief Block until the underlying pool becomes idle. */
    void wait_idle() override;

    /** @brief Initial worker thread count. */
    std::size_t threads() const noexcept;

    /** @brief Maximum worker thread count. */
    std::size_t max_threads() const noexcept;

  private:
    std::unique_ptr<vix::threadpool::ThreadPool> pool_;
    std::size_t threads_{0};
    std::size_t max_threads_{0};
  };

  /**
   * @brief Factory helper to create a thread pool executor.
   *
   * @param threads Initial worker thread count.
   * @param maxThreads Maximum worker thread count.
   * @param defaultPriority Default task priority.
   * @return Unique executor instance.
   */
  std::unique_ptr<vix::executor::IExecutor> make_threadpool_executor(
      std::size_t threads,
      std::size_t maxThreads,
      int defaultPriority);

} // namespace vix::experimental

#endif // VIX_THREAD_POOL_EXECUTOR_HPP
