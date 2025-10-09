/**
 * @file ThreadPool.hpp
 * @brief Priority-based, production-ready task executor used across Vix.cpp.
 *
 * @details
 * `Vix::ThreadPool` provides a bounded, grow-on-demand pool of worker threads
 * that execute submitted tasks according to **priority**. It exposes a
 * futures-based `enqueue()` API for regular tasks and a `periodicTask()` API
 * for recurring jobs (metrics, housekeeping, etc.). The pool is safe for use
 * from multiple producer threads.
 *
 * Key features
 * - **Priority scheduling**: higher `priority` values run before lower ones.
 * - **Futures API**: `enqueue()` returns `std::future<R>` of the callable.
 * - **Timeout telemetry**: optional per-task timeout window logs slow tasks and
 *   increments a global `timedOutTasks` counter (non-cancelling).
 * - **Elastic sizing**: the pool can spawn additional workers up to
 *   `maxThreads` when the queue is under pressure.
 * - **Periodic jobs**: a capped set of dedicated periodic threads schedule
 *   recurring work by reusing the pool (no long blocking inside workers).
 *
 * Threading model
 * - **Workers**: vector of `std::thread` that pop from a priority queue.
 * - **Periodic schedulers**: separate threads that submit work into the pool at
 *   a fixed interval (limited by `maxPeriodicThreads`).
 * - **Mutex/condvar**: protect the priority queue and coordinate producers
 *   vs. consumers without busy-waiting.
 *
 * Lifetime & shutdown
 * - The destructor requests stop, wakes all waiters, and joins workers and
 *   periodic threads. In-flight tasks are allowed to finish.
 * - Call `stopPeriodicTasks()` to stop only periodic schedulers (workers keep
 *   running) when the application wants to quiesce background jobs.
 *
 * Usage example
 * @code{.cpp}
 * Vix::ThreadPool pool(4, 8, 1); // threads, maxThreads, defaultPrio
 * auto fut = pool.enqueue(10, std::chrono::milliseconds(200), [] {}); // do work
 * fut.get(); // wait for completion
 * pool.periodicTask(1, [] {}, std::chrono::seconds(5)); // metrics/housekeeping
 * // ... later
 * pool.stopPeriodicTasks();
 * @endcode
 */

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
#include <limits>
#include <pthread.h>

#include <vix/utils/Logger.hpp>

namespace Vix
{
    /**
     * @brief Unit of scheduled work with a monotonic priority.
     * @note Larger `priority` means earlier execution.
     */
    struct Task
    {
        std::function<void()> func; //!< Work to execute.
        int priority;               //!< Higher runs sooner.

        Task(std::function<void()> f, int p) : func(std::move(f)), priority(p) {}
        Task() : func(nullptr), priority(0) {}

        /**
         * @brief Ordering for std::priority_queue (max-heap by priority).
         * Higher priority comes first, hence the inverted comparison.
         */
        bool operator<(const Task &other) const
        {
            // high priority => executed before (so > for priority_queue max-heap)
            return priority > other.priority;
        }
    };

    /** @brief RAII helper to track currently running tasks. */
    struct TaskGuard
    {
        std::atomic<int> &counter;
        explicit TaskGuard(std::atomic<int> &c) : counter(c) { ++counter; }
        ~TaskGuard() { --counter; }
    };

    /** @brief Lightweight, snapshot-style metrics exposed by the pool. */
    struct Metrics
    {
        int pendingTasks;  //!< Number of tasks currently queued.
        int activeTasks;   //!< Number of tasks currently executing.
        int timedOutTasks; //!< Cumulative count of tasks that exceeded their configured timeout.
    };

    /**
     * @brief Thread-local identifier assigned by the pool for logging/telemetry.
     * @details Extern is defined in the corresponding implementation unit.
     */
    extern thread_local int threadId;

    /**
     * @class ThreadPool
     * @brief Priority-based task executor with futures API and periodic scheduling.
     *
     * @section config Configuration knobs
     * - `threadCount`: initial number of worker threads to spawn.
     * - `maxThreadCount` (aka `maxThreads`): hard ceiling for worker threads;
     *   the pool may grow lazily up to this limit on pressure.
     * - `priority`: default priority used by `enqueue(f, args...)` overload.
     * - `maxPeriodic`: maximum simultaneously active periodic scheduler threads.
     */
    class ThreadPool
    {
    private:
        std::vector<std::thread> workers;               //!< Worker threads.
        std::priority_queue<Task> tasks;                //!< Ready queue (highest priority first).
        std::mutex m;                                   //!< Protects `tasks` and growth decisions.
        std::condition_variable condition;              //!< Producer/consumer coordination.
        std::atomic<bool> stop;                         //!< Global stop for workers.
        std::atomic<bool> stopPeriodic;                 //!< Stop flag for periodic schedulers.
        std::size_t maxThreads;                         //!< Upper bound for worker count.
        std::atomic<int> activeTasks;                   //!< Number of tasks executing right now.
        std::vector<std::thread> periodicWorkers;       //!< Periodic scheduler threads.
        int threadPriority;                             //!< Default priority for enqueue(f,...).
        std::size_t maxPeriodicThreads;                 //!< Cap for concurrent periodic schedulers.
        std::atomic<std::size_t> activePeriodicThreads; //!< Current number of running periodic schedulers.
        std::atomic<int> tasksTimedOut;                 //!< Cumulative timeout counter.

        /**
         * @brief Best-effort CPU affinity (Linux only). No-op elsewhere.
         */
        void setThreadAffinity(std::size_t id)
        {
#ifdef __linux__
            if (maxThreads <= 1)
                return;

            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);

            const unsigned hc = std::thread::hardware_concurrency();
            const unsigned denom = (hc == 0u) ? 1u : hc;
            const unsigned coreU = static_cast<unsigned>(id % denom);
            const int core = static_cast<int>(coreU);

            CPU_SET(core, &cpuset);

            const int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
            if (ret != 0)
            {
                auto &log = Vix::Logger::getInstance();
                log.log(Vix::Logger::Level::WARN,
                        "[ThreadPool][Thread {}] Failed to set thread affinity, error: {}", threadId, ret);
            }
#else
            (void)id; // avoid -Wunused-parameter on other platforms
#endif
        }

        /**
         * @brief Spawn a single worker thread that consumes tasks until stop.
         */
        void createThread(std::size_t id)
        {
            workers.emplace_back(
                [this, id]()
                {
                    threadId = (id > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                                   ? std::numeric_limits<int>::max()
                                   : static_cast<int>(id);

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

                            // priority_queue::top() is const& ; we copy then we pop.
                            task = tasks.top();
                            tasks.pop();
                        }

                        TaskGuard guard(activeTasks);

                        try
                        {
                            if (task.func)
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
        /**
         * @brief Construct a thread pool.
         * @param threadCount    Initial number of worker threads to spawn.
         * @param maxThreadCount Upper bound on worker threads; pool may grow up to this.
         * @param priority       Default priority used by the simpler `enqueue()` overloads.
         * @param maxPeriodic    Maximum concurrently active periodic scheduler threads.
         */
        ThreadPool(std::size_t threadCount, std::size_t maxThreadCount, int priority, std::size_t maxPeriodic = 4)
            : workers(), tasks(), m(), condition(), stop(false), stopPeriodic(false),
              maxThreads(maxThreadCount), activeTasks(0), periodicWorkers(), threadPriority(priority),
              maxPeriodicThreads(maxPeriodic), activePeriodicThreads(0), tasksTimedOut(0)
        {
            for (std::size_t i = 0; i < threadCount; ++i)
                createThread(i);
        }

        /**
         * @brief Snapshot pool metrics (non-blocking).
         */
        Metrics getMetrics()
        {
            std::lock_guard<std::mutex> lock(m);
            return Metrics{
                static_cast<int>(tasks.size()), // pending tasks
                activeTasks.load(),             // current tasks
                tasksTimedOut.load()            // timeout tasks
            };
        }

        /**
         * @brief Enqueue a callable with explicit priority and timeout.
         * @tparam F Callable type (invoked with `Args...`).
         * @tparam Args Parameter pack forwarded to the callable.
         * @param priority Higher executes sooner relative to other tasks.
         * @param timeout  Soft budget for execution time; if exceeded, a warning
         *                 is logged and `timedOutTasks` is incremented. The task
         *                 is not preempted or cancelled.
         * @return `std::future<R>` where `R = std::invoke_result_t<F, Args...>`.
         */
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
                        const auto start = std::chrono::steady_clock::now();

                        try
                        {
                            (*task)();
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

                        if (timeout.count() > 0)
                        {
                            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - start);
                            if (elapsed.count() > timeout.count())
                            {
                                log.log(Vix::Logger::Level::WARN,
                                        "[ThreadPool][Timeout] Thread {} exceeded timeout of {} ms (actual: {} ms)",
                                        threadId, timeout.count(), elapsed.count());
                                tasksTimedOut.fetch_add(1);
                            }
                        }
                    },
                    priority});

                if (workers.size() < maxThreads)
                    createThread(workers.size());
            }

            condition.notify_one();
            return res;
        }

        /** @overload Enqueue with explicit priority and no timeout. */
        template <class F, class... Args>
        auto enqueue(int priority, F &&f, Args &&...args)
        {
            return enqueue(priority, std::chrono::milliseconds(0), std::forward<F>(f), std::forward<Args>(args)...);
        }

        /** @overload Enqueue using the pool's default priority. */
        template <class F, class... Args>
        auto enqueue(F &&f, Args &&...args)
            -> std::future<typename std::invoke_result<F, Args...>::type>
        {
            return enqueue(threadPriority, std::forward<F>(f), std::forward<Args>(args)...);
        }

        /**
         * @brief Schedule a recurring task executed via the pool at a fixed interval.
         * @param priority Priority used for each scheduled run.
         * @param func     Callable to run.
         * @param interval Period between successive runs.
         *
         * @details Spawns (at most) `maxPeriodicThreads` scheduler threads.
         * Each scheduler submits work using `enqueue()` and then sleeps for
         * `interval`. If a run exceeds `interval`, a warning is logged, but the
         * next run is not skipped (simple fixed-delay schedule).
         */
        void periodicTask(int priority, std::function<void()> func, std::chrono::milliseconds interval)
        {
            // Limit the number of active periodic threads
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

        /** @brief True if no task is executing and the queue is empty. */
        bool isIdle()
        {
            std::lock_guard<std::mutex> lock(m);
            return activeTasks.load() == 0 && tasks.empty();
        }

        /** @brief Stop only periodic schedulers; workers remain available. */
        void stopPeriodicTasks()
        {
            stopPeriodic = true;
            condition.notify_all();
        }

        /**
         * @brief Destructor: cooperatively stop and join all threads.
         *
         * @details Signals `stop` and `stopPeriodic`, wakes all waiting
         * workers, then joins both worker and periodic threads. In-flight
         * tasks are allowed to complete.
         */
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

#endif // VIX_THREADPOOL_HPP
