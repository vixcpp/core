/**
 *
 *  @file ThreadPoolExecutor.cpp
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
#include <vix/experimental/ThreadPoolExecutor.hpp>

namespace vix::experimental
{
  ThreadPoolExecutor::ThreadPoolExecutor(
      std::size_t threads,
      std::size_t maxThreads,
      int defaultPriority)
      : pool_(std::make_unique<vix::threadpool::ThreadPool>(threads, maxThreads, defaultPriority)),
        threads_(threads),
        max_threads_(maxThreads) {}

  std::size_t ThreadPoolExecutor::threads() const noexcept
  {
    return threads_;
  }

  std::size_t ThreadPoolExecutor::max_threads() const noexcept
  {
    return max_threads_;
  }

  bool ThreadPoolExecutor::post(std::function<void()> fn, vix::executor::TaskOptions opt)
  {
    try
    {
      if (opt.timeout.count() > 0)
      {
        (void)pool_->enqueue(opt.priority, opt.timeout, std::move(fn));
      }
      else
      {
        (void)pool_->enqueue(opt.priority, std::move(fn));
      }
      return true;
    }
    catch (...)
    {
      return false;
    }
  }

  vix::executor::Metrics ThreadPoolExecutor::metrics() const
  {
    auto m = pool_->getMetrics();
    return vix::executor::Metrics{m.pendingTasks, m.activeTasks, m.timedOutTasks};
  }

  void ThreadPoolExecutor::wait_idle()
  {
    pool_->waitUntilIdle();
  }

  std::unique_ptr<vix::executor::IExecutor>
  make_threadpool_executor(std::size_t threads, std::size_t maxThreads, int defaultPriority)
  {
    return std::unique_ptr<vix::executor::IExecutor>(
        new ThreadPoolExecutor(threads, maxThreads, defaultPriority));
  }
}
