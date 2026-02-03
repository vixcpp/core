/**
 *
 * @file IExecutor.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_IEXECUTOR_HPP
#define VIX_IEXECUTOR_HPP

#include <future>
#include <functional>
#include <type_traits>
#include <memory>

#include <vix/executor/TaskOptions.hpp>
#include <vix/executor/Metrics.hpp>

namespace vix::executor
{

  /**
   * @brief Generic task execution interface.
   *
   * Provides asynchronous task posting, metrics access, and idle synchronization.
   */
  struct IExecutor
  {
    /** @brief Virtual destructor. */
    virtual ~IExecutor() = default;

    /** @brief Post a task for execution. */
    virtual bool post(std::function<void()> fn, TaskOptions opt = {}) = 0;

    /** @brief Return executor metrics snapshot. */
    virtual vix::executor::Metrics metrics() const = 0;

    /** @brief Block until the executor becomes idle. */
    virtual void wait_idle() = 0;

    /**
     * @brief Submit a callable and obtain a future to its result.
     *
     * @param f Callable.
     * @param args Callable arguments.
     * @param opt Task options.
     * @return std::future for the callable result.
     */
    template <class F, class... Args>
    auto submit(F &&f, Args &&...args, TaskOptions opt = {})
        -> std::future<std::invoke_result_t<F, Args...>>
    {
      using R = std::invoke_result_t<F, Args...>;
      auto task = std::make_shared<std::packaged_task<R()>>(
          std::bind(std::forward<F>(f), std::forward<Args>(args)...));
      auto fut = task->get_future();
      if (!post([task]
                { (*task)(); }, opt))
      {
        std::promise<R> p;
        p.set_exception(std::make_exception_ptr(
            std::runtime_error("submit rejected by executor")));
        return p.get_future();
      }
      return fut;
    }
  };

} // namespace vix::executor

#endif // VIX_IEXECUTOR_HPP
