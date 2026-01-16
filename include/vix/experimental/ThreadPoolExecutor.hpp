/**
 *
 *  @file ThreadPoolExecutor.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
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
  class ThreadPoolExecutor final : public vix::executor::IExecutor
  {
  public:
    explicit ThreadPoolExecutor(
        std::size_t threads,
        std::size_t maxThreads,
        int defaultPriority);

    bool post(std::function<void()> fn, vix::executor::TaskOptions opt = {}) override;
    vix::executor::Metrics metrics() const override;
    void wait_idle() override;

    std::size_t threads() const noexcept;
    std::size_t max_threads() const noexcept;

  private:
    std::unique_ptr<vix::threadpool::ThreadPool> pool_;
    std::size_t threads_{0};
    std::size_t max_threads_{0};
  };

  std::unique_ptr<vix::executor::IExecutor> make_threadpool_executor(
      std::size_t threads,
      std::size_t maxThreads,
      int defaultPriority);
}

#endif
