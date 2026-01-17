/**
 *
 *  @file ThreadPool.hpp
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
#ifndef VIX_THREAD_POOL_HPP
#define VIX_THREAD_POOL_HPP

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <limits>

#ifdef __linux__
#include <pthread.h>
#endif

#include <vix/utils/Logger.hpp>
#include <vix/threadpool/TaskCmp.hpp>
#include <vix/threadpool/TaskGuard.hpp>

namespace vix::threadpool
{
  using Logger = vix::utils::Logger;

  inline Logger &log()
  {
    return Logger::getInstance();
  }

  struct Metrics
  {
    std::uint64_t pendingTasks;
    std::uint64_t activeTasks;
    std::uint64_t timedOutTasks;
  };

  extern thread_local int threadId;

  class ThreadPool
  {
  private:
    std::vector<std::thread> workers;
    std::priority_queue<Task, std::vector<Task>, TaskCmp> tasks;
    std::atomic<std::uint64_t> nextSeq{0};
    std::mutex m;
    std::condition_variable condition;
    std::atomic<bool> stop;
    std::atomic<bool> stopPeriodic;
    std::size_t maxThreads;
    std::atomic<std::uint64_t> activeTasks{0};
    std::vector<std::thread> periodicWorkers;
    int threadPriority;
    std::size_t maxPeriodicThreads;
    std::atomic<std::size_t> activePeriodicThreads;
    std::atomic<std::uint64_t> tasksTimedOut;
    std::mutex mPeriodic;
    std::condition_variable cvPeriodic;
    std::condition_variable cvIdle;

    void setThreadAffinity(std::size_t id)
    {
#ifdef __linux__
      if (maxThreads <= 1)
        return;

      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      const std::size_t hc = std::thread::hardware_concurrency();
      const std::size_t denom = (hc == 0u) ? 1u : hc;
      const std::size_t coreIndex = id % denom;
      CPU_SET(coreIndex, &cpuset);
      const int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
      if (ret != 0)
      {
        log().log(Logger::Level::WARN,
                  "[ThreadPool][Thread {}] Failed to set thread affinity, error: {}",
                  threadId, ret);
      }
#else
      (void)id;
#endif
    }

    void createThread(std::size_t id)
    {
      workers.emplace_back(
          [this, id]()
          {
            threadId = (id > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                        ? std::numeric_limits<int>::max()
                        : static_cast<int>(id);

            setThreadAffinity(id);

            while (true)
            {
                Task task;
                {
                    std::unique_lock<std::mutex> lock(m);
                    condition.wait(lock, [this]{ return stop || !tasks.empty(); });
                    if (stop && tasks.empty())
                        return;
                    task = tasks.top();
                    tasks.pop();
                }

                {
                    TaskGuard guard(activeTasks);
                    if (task.func)
                        task.func();
                }

                {
                    std::lock_guard<std::mutex> lock(m);
                    if (tasks.empty() && activeTasks.load(std::memory_order_relaxed) == 0)
                    {
                        cvIdle.notify_all();
                    }
                }

            } });
    }

  public:
    ThreadPool(
        std::size_t threadCount,
        std::size_t maxThreadCount,
        int priority,
        std::size_t maxPeriodic = 4)
        : workers(),
          tasks(),
          nextSeq(0),
          m(),
          condition(),
          stop(false),
          stopPeriodic(false),
          maxThreads(maxThreadCount > 0 ? maxThreadCount : 1),
          activeTasks(0),
          periodicWorkers(),
          threadPriority(priority),
          maxPeriodicThreads(maxPeriodic == 0 ? 1 : maxPeriodic),
          activePeriodicThreads(0),
          tasksTimedOut(0),
          mPeriodic(),
          cvPeriodic(),
          cvIdle()
    {
      if (threadCount == 0)
        threadCount = 1;
      if (threadCount > maxThreads)
        threadCount = maxThreads;

      workers.reserve(threadCount);

      try
      {
        for (std::size_t i = 0; i < threadCount; ++i)
        {
          createThread(i);
        }
      }
      catch (...)
      {
        {
          std::lock_guard<std::mutex> lock(m);
          stop = true;
          stopPeriodic = true;
        }
        condition.notify_all();

        for (auto &t : workers)
          if (t.joinable())
            t.join();
        for (auto &t : periodicWorkers)
          if (t.joinable())
            t.join();

        throw;
      }

      log().log(Logger::Level::DEBUG,
                "[ThreadPool] started (threads={}, max={}, prio={}, periodic={})",
                threadCount, maxThreads, threadPriority, maxPeriodicThreads);
    }

    [[nodiscard]] Metrics getMetrics()
    {
      std::lock_guard<std::mutex> lock(m);
      return Metrics{
          static_cast<std::uint64_t>(tasks.size()),
          activeTasks.load(std::memory_order_relaxed),
          tasksTimedOut.load(std::memory_order_relaxed)};
    }

    template <class F, class... Args>
    auto enqueue(
        int priority,
        std::chrono::milliseconds timeout,
        F &&f, Args &&...args)
        -> std::future<typename std::invoke_result<F, Args...>::type>
    {
      using ReturnType = std::invoke_result<F, Args...>::type;

      if (stop.load(std::memory_order_relaxed))
      {
        throw std::runtime_error("ThreadPool is stopped; cannot enqueue new tasks");
      }

      auto task = std::make_shared<std::packaged_task<ReturnType()>>(
          std::bind(std::forward<F>(f), std::forward<Args>(args)...));
      std::future<ReturnType> res = task->get_future();
      const std::uint64_t seq = nextSeq.fetch_add(1, std::memory_order_relaxed);

      {
        std::unique_lock<std::mutex> lock(m);
        tasks.push(
            Task{
                [task, timeout, this]()
                {
                  const auto start = std::chrono::steady_clock::now();
                  (*task)();

                  if (timeout.count() > 0)
                  {
                    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - start);
                    if (elapsed.count() > timeout.count())
                    {

                      auto &log = Logger::getInstance();
                      log.log(Logger::Level::WARN,
                              "[ThreadPool][Timeout] Thread {} exceeded timeout of {} ms (actual: {} ms)",
                              threadId, timeout.count(), elapsed.count());
                      tasksTimedOut.fetch_add(1, std::memory_order_relaxed);
                    }
                  }
                },
                priority,
                seq});

        const std::uint64_t wcount = workers.size();
        const bool saturated = (activeTasks.load(std::memory_order_relaxed) >= static_cast<std::uint64_t>(wcount));
        const bool backlog = (tasks.size() > wcount);

        if (wcount < maxThreads && saturated && backlog)
        {
          createThread(wcount);
        }
      }

      condition.notify_one();
      return res;
    }

    template <class F, class... Args>
    auto enqueue(int priority, F &&f, Args &&...args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
      return enqueue(
          priority, std::chrono::milliseconds{0},
          std::forward<F>(f), std::forward<Args>(args)...);
    }

    template <class F, class... Args>
    auto enqueue(F &&f, Args &&...args)
        -> std::future<std::invoke_result_t<F, Args...>>
    {
      return enqueue(
          threadPriority, std::chrono::milliseconds{0},
          std::forward<F>(f), std::forward<Args>(args)...);
    }

    void periodicTask(int priority, std::function<void()> func, std::chrono::milliseconds interval)
    {
      {
        std::unique_lock<std::mutex> lock(mPeriodic);
        cvPeriodic.wait(
            lock, [this]
            { return stopPeriodic.load(std::memory_order_relaxed) || activePeriodicThreads.load(std::memory_order_relaxed) < maxPeriodicThreads; });

        if (stopPeriodic.load(std::memory_order_relaxed))
          return;

        activePeriodicThreads.fetch_add(1, std::memory_order_relaxed);
      }

      try
      {
        periodicWorkers.emplace_back(
            [this, priority, func, interval]()
            {
              threadId = 100000 + static_cast<int>(
                                      std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0x7FFF);

              auto &log = Logger::getInstance();
              auto next = std::chrono::steady_clock::now() + interval;

              while (!stopPeriodic.load(std::memory_order_relaxed))
              {
                auto wrapped = [func, &log]()
                {
                  try
                  {
                    func();
                  }
                  catch (const std::exception &e)
                  {
                    log.log(Logger::Level::ERROR,
                            "[ThreadPool][PeriodicException] Exception in periodic task: {}", e.what());
                    throw;
                  }
                  catch (...)
                  {
                    log.log(Logger::Level::ERROR,
                            "[ThreadPool][PeriodicException] Unknown exception in periodic task");
                    throw;
                  }
                };

                std::future<void> future;
                try
                {
                  future = enqueue(priority, std::chrono::milliseconds{0}, std::move(wrapped));
                }
                catch (const std::exception &e)
                {
                  log.log(Logger::Level::WARN,
                          "[ThreadPool][Periodic] enqueue() failed, stopping scheduler: {}", e.what());
                  break;
                }
                catch (...)
                {
                  log.log(Logger::Level::WARN,
                          "[ThreadPool][Periodic] enqueue() failed with unknown error, stopping scheduler");
                  break;
                }

                {
                  std::unique_lock<std::mutex> lock(mPeriodic);
                  if (cvPeriodic.wait_until(lock, next, [this]
                                            { return stopPeriodic.load(std::memory_order_relaxed); }))
                  {
                    break;
                  }
                }

                if (future.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready)
                {
                  log.log(Logger::Level::WARN,
                          "[ThreadPool][PeriodicTimeout] Thread {} periodic task exceeded interval of {} ms",
                          threadId, interval.count());
                }

                next += interval;
              }

              activePeriodicThreads.fetch_sub(1, std::memory_order_relaxed);
              cvPeriodic.notify_one();
            });
      }
      catch (...)
      {
        activePeriodicThreads.fetch_sub(1, std::memory_order_relaxed);
        cvPeriodic.notify_one();
        throw;
      }
    }

    [[nodiscard]] bool isIdle() noexcept
    {
      std::lock_guard<std::mutex> lock(m);
      return activeTasks.load(std::memory_order_relaxed) == 0 && tasks.empty();
    }

    void waitUntilIdle()
    {
      std::unique_lock<std::mutex> lock(m);
      cvIdle.wait(lock, [this]
                  { return tasks.empty() && activeTasks.load(std::memory_order_relaxed) == 0; });
    }

    void stopPeriodicTasks() noexcept
    {
      stopPeriodic.store(true, std::memory_order_relaxed);
      condition.notify_all();
    }

    ~ThreadPool() noexcept
    {
      {
        std::lock_guard<std::mutex> lk(m);
        stop.store(true, std::memory_order_relaxed);
        stopPeriodic.store(true, std::memory_order_relaxed);
      }

      condition.notify_all();
      cvPeriodic.notify_all();

      for (auto &pworker : periodicWorkers)
      {
        if (pworker.joinable())
          pworker.join();
      }

      for (auto &worker : workers)
      {
        if (worker.joinable())
          worker.join();
      }
    }
  };

} // namespace vix::threadpool

#endif // VIX_THREADPOOL_HPP
