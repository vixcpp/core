#ifndef VIX_THREADPOOL_HPP
#define VIX_THREADPOOL_HPP

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <future>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <pthread.h>

#include "../utils/Logger.hpp"

namespace Vix
{

    struct Task
    {
        std::function<void()> func;
        int priority;

        Task(std::function<void()> f, int p) : func(f), priority(p) {}
        Task() : func(nullptr), priority(0) {}

        bool operator<(const Task &other) const
        {
            return priority > other.priority;
        }
    };

    struct TaskGuard
    {
        std::atomic<int> &counter;
        TaskGuard(std::atomic<int> &c) : counter(c) { ++counter; }
        ~TaskGuard() { --counter; }
    };

    struct Metrics
    {
        int pendingTasks;
        int activeTasks;
        int timedOutTasks;
    };

    extern thread_local int threadId;

    class ThreadPool
    {
    private:
        std::vector<std::thread> workers;
        std::priority_queue<Task> tasks;
        std::mutex m;
        std::condition_variable condition;
        std::atomic<bool> stop;
        std::atomic<bool> stopPeriodic;
        size_t maxThreads;
        std::atomic<int> activeTasks;
        std::vector<std::thread> periodicWorkers;
        int threadPriority;
        size_t maxPeriodicThreads;
        std::atomic<size_t> activePeriodicThreads;
        std::atomic<int> tasksTimedOut{0};

        void setThreadAffinity(int id)
        {
#ifdef __linux__
            if (maxThreads <= 1)
                return;

            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            int core = id % std::thread::hardware_concurrency();
            CPU_SET(core, &cpuset);

            int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
            if (ret != 0)
            {
                auto &log = Vix::Logger::getInstance();
                log.log(Vix::Logger::Level::WARN,
                        "[ThreadPool][Thread {}] Failed to set thread affinity, error: {}", id, ret);
            }
#endif
        }

        void createThread(int id)
        {
            workers.emplace_back(
                [this, id]()
                {
                    threadId = id;
                    setThreadAffinity(id);
                    auto &log = Vix::Logger::getInstance();

                    while (true)
                    {
                        Task task;
                        {
                            std::unique_lock<std::mutex> lock(m);
                            condition.wait(lock, [this]
                                           { return stop || !tasks.empty(); });
                            if (stop && tasks.empty())
                                return;
                            task = std::move(tasks.top());
                            tasks.pop();
                        }

                        TaskGuard guard(activeTasks);

                        try
                        {
                            task.func();
                        }
                        catch (const std::exception &e)
                        {
                            log.log(Vix::Logger::Level::ERROR,
                                    "[ThreadPool][Thread {}] Exception in task: {}", threadId, e.what());
                        }
                        catch (...)
                        {
                            log.log(Vix::Logger::Level::ERROR,
                                    "[ThreadPool][Thread {}] Unknown exception in task", threadId);
                        }

                        condition.notify_one();
                    }
                });
        }

    public:
        ThreadPool(size_t threadCount, size_t maxThreadCount, int priority, size_t maxPeriodic = 4)
            : stop(false),
              stopPeriodic(false),
              maxThreads(maxThreadCount),
              activeTasks(0),
              threadPriority(priority),
              maxPeriodicThreads(maxPeriodic),
              activePeriodicThreads(0)
        {
            for (size_t i = 0; i < threadCount; ++i)
                createThread(i);
        }

        Metrics getMetrics()
        {
            std::lock_guard<std::mutex> lock(m);
            return Metrics{
                static_cast<int>(tasks.size()), // pending tasks
                activeTasks.load(),             // current tasks
                tasksTimedOut.load()            // timeout tasks
            };
        }

        template <class F, class... Args>
        auto enqueue(int priority, std::chrono::milliseconds timeout, F &&f, Args &&...args)
            -> std::future<typename std::invoke_result<F, Args...>::type>
        {
            using ReturnType = typename std::invoke_result<F, Args...>::type;
            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...));

            std::future<ReturnType> res = task->get_future();

            {
                std::unique_lock<std::mutex> lock(m);

                tasks.push(Task{
                    [task, timeout, this]()
                    {
                        auto &log = Vix::Logger::getInstance();
                        auto start = std::chrono::steady_clock::now();

                        std::thread t([task]()
                                      { (*task)(); });

                        if (timeout.count() > 0)
                        {
                            auto future = std::async(std::launch::async, [task]()
                                                     { (*task)(); });

                            if (future.wait_for(timeout) == std::future_status::timeout)
                            {
                                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                    std::chrono::steady_clock::now() - start);

                                if (elapsed.count() > timeout.count())
                                {
                                    log.log(Vix::Logger::Level::WARN,
                                            "[ThreadPool][Timeout] Thread {} exceeded timeout of {} ms (actual: {} ms)",
                                            threadId, timeout.count(), elapsed.count());
                                    tasksTimedOut.fetch_add(1);
                                }
                            }
                        }
                        else
                        {
                            (*task)();
                        }
                    },
                    priority});

                if (workers.size() < maxThreads)
                    createThread(workers.size());
            }

            condition.notify_one();
            return res;
        }

        template <class F, class... Args>
        auto enqueue(int priority, F &&f, Args &&...args)
        {
            return enqueue(priority, std::chrono::milliseconds(0), std::forward<F>(f), std::forward<Args>(args)...);
        }

        template <class F, class... Args>
        auto enqueue(F &&f, Args &&...args)
            -> std::future<typename std::invoke_result<F, Args...>::type>
        {
            return enqueue(threadPriority, std::forward<F>(f), std::forward<Args>(args)...);
        }

        void periodicTask(int priority, std::function<void()> func, std::chrono::milliseconds interval)
        {
            while (activePeriodicThreads.load() >= maxPeriodicThreads)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));

            activePeriodicThreads.fetch_add(1);

            periodicWorkers.emplace_back([this, priority, func, interval]()
                                         {
            auto &log = Vix::Logger::getInstance();

            while (!stopPeriodic)
            {
                try
                {
                    auto future = enqueue(priority, func);

                    if (future.wait_for(interval) == std::future_status::timeout)
                    {
                        log.log(Vix::Logger::Level::WARN,
                                "[ThreadPool][PeriodicTimeout] Thread {} periodic task exceeded interval of {} ms",
                                threadId, interval.count());
                    }
                }
                catch (const std::exception &e)
                {
                    log.log(Vix::Logger::Level::ERROR,
                            "[ThreadPool][PeriodicException] Exception in periodic task: {}", e.what());
                }
                catch (...)
                {
                    log.log(Vix::Logger::Level::ERROR,
                            "[ThreadPool][PeriodicException] Unknown exception in periodic task");
                }

                std::this_thread::sleep_for(interval);
            }

            activePeriodicThreads.fetch_sub(1); });
        }

        bool isIdle()
        {
            std::lock_guard<std::mutex> lock(m);
            return activeTasks.load() == 0 && tasks.empty();
        }

        void stopPeriodicTasks()
        {
            stopPeriodic = true;
            condition.notify_all();
        }

        ~ThreadPool()
        {
            {
                std::unique_lock<std::mutex> lock(m);
                stop = true;
                stopPeriodic = true;
            }
            condition.notify_all();

            for (auto &worker : workers)
                if (worker.joinable())
                    worker.join();

            for (auto &pworker : periodicWorkers)
                if (pworker.joinable())
                    pworker.join();
        }
    };

} // namespace Vix

#endif
