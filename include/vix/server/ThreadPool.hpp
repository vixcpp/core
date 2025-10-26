#ifndef VIX_THREAD_POOL_HPP
#define VIX_THREAD_POOL_HPP
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

#ifdef __linux__
#include <pthread.h>
#endif

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
        std::uint64_t seq;          //!< filled at the tail, croissant

        Task(std::function<void()> f, int p, std::uint64_t s)
            : func(std::move(f)), priority(p), seq(s) {}
        Task()
            : func(nullptr), priority(0), seq(0) {}

        // /**
        //  * @brief Ordering for std::priority_queue (max-heap by priority).
        //  * Higher priority comes first, hence the inverted comparison.
        //  */
        // bool operator<(const Task &other) const
        // {
        //     // high priority => executed before (so > for priority_queue max-heap)
        //     return priority < other.priority;
        // }
    };

    struct TaskCmp
    {
        bool operator()(const Task &a, const Task &b) const noexcept
        {
            if (a.priority != b.priority)
            {
                return a.priority < b.priority; // plus grand = priorite (plus haut = avant)
            }
            return a.seq > b.seq; // plus ancien (seq plus petit) d'abord (plus ancien = avant)
        }
    };

    /** @brief RAII helper to track currently running tasks. */
    template <class T>
    struct TaskGuard
    {
        static_assert(std::is_integral_v<T>, "TaskGuard requires an integral counter type");

        std::atomic<T> &counter;

        explicit TaskGuard(std::atomic<T> &c) : counter(c)
        {
            counter.fetch_add(1, std::memory_order_relaxed);
        }

        ~TaskGuard()
        {
            counter.fetch_sub(1, std::memory_order_relaxed);
        }
    };

    template <class T>
    TaskGuard(std::atomic<T> &) -> TaskGuard<T>;

    /** @brief Lightweight, snapshot-style metrics exposed by the pool. */
    struct Metrics
    {
        std::uint64_t pendingTasks;  //!< Number of tasks currently queued.
        std::uint64_t activeTasks;   //!< Number of tasks currently executing.
        std::uint64_t timedOutTasks; //!< Cumulative count of tasks that exceeded their configured timeout.
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
        // Threads & tasks
        std::vector<std::thread> workers;                            //!< Worker threads.
        std::priority_queue<Task, std::vector<Task>, TaskCmp> tasks; //!< Ready queue (highest priority first).
        std::atomic<std::uint64_t> nextSeq{0};                       // arrival number (FIFO for equal priority)

        // Main synchronization
        std::mutex m;                      //!< Protects `tasks` and growth decisions.
        std::condition_variable condition; //!< Producer/consumer coordination.

        // Stop flags
        std::atomic<bool> stop;         //!< Global stop for workers.
        std::atomic<bool> stopPeriodic; //!< Stop flag for periodic schedulers.

        // Capacities / counters
        std::size_t maxThreads;                         //!< Upper bound for worker count.
        std::atomic<std::uint64_t> activeTasks{0};      //!< Number of tasks executing right now.
        std::vector<std::thread> periodicWorkers;       //!< Periodic scheduler threads.
        int threadPriority;                             //!< Default priority for enqueue(f,...).
        std::size_t maxPeriodicThreads;                 //!< Cap for concurrent periodic schedulers.
        std::atomic<std::size_t> activePeriodicThreads; //!< Current number of running periodic schedulers.
        std::atomic<std::uint64_t> tasksTimedOut;       //!< Cumulative timeout counter.

        // Periodic synchronization
        std::mutex mPeriodic;
        std::condition_variable cvPeriodic;
        std::condition_variable cvIdle;

        /**
         * @brief Best-effort CPU affinity (Linux only). No-op elsewhere.
         */
        void setThreadAffinity(std::size_t id)
        {
#ifdef __linux__
            if (maxThreads <= 1)
                return;

            cpu_set_t cpuset;  // struct du noyau linux qui represente un ensemble de coeurs
            CPU_ZERO(&cpuset); // vider (aucun coeur selectionne au depart)

            const unsigned hc = std::thread::hardware_concurrency();
            const unsigned denom = (hc == 0u) ? 1u : hc;
            const unsigned coreU = static_cast<unsigned>(id % denom);
            const int core = static_cast<int>(coreU);

            CPU_SET(static_cast<int>(core), &cpuset);

            const int ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
            if (ret != 0)
            {
                auto &log = Vix::Logger::getInstance();
                log.log(Vix::Logger::Level::WARN,
                        "[ThreadPool][Thread {}] Failed to set thread affinity, error: {}", threadId, ret);
            }
#else
            (void)id; // avoid -Wunused-parameter on other platforms(eviter un warning sur windows/macos)
#endif
        }

        /**
         * @brief Spawn a single worker thread that consumes tasks until stop.
         */
        void createThread(std::size_t id)
        {
            workers.emplace_back([this, id]()
                                 {
            // Readable ID for logs (thread-local)
            threadId = (id > static_cast<std::size_t>(std::numeric_limits<int>::max()))
                        ? std::numeric_limits<int>::max()
                        : static_cast<int>(id);

            setThreadAffinity(id);
            auto& log = Vix::Logger::getInstance();
            (void)log; // avoid warning if unused

            while (true)
            {
                Task task;
                {
                    std::unique_lock<std::mutex> lock(m);
                    // Wake up when stop == true or task available
                    condition.wait(lock, [this]{ return stop || !tasks.empty(); });

                    // Graceful shutdown: exit if stop requested and no tasks left
                    if (stop && tasks.empty())
                        return;

                    // priority_queue::top() returns const&, so copy then pop
                    task = tasks.top();
                    tasks.pop();
                }

                // Execute the task (active task counter managed via RAII)
                {
                    TaskGuard guard(activeTasks); // ++ on entry, -- on exit of this block
                    if (task.func)
                        task.func(); // let packaged_task propagate exceptions to future
                } // <-- activeTasks may return to 0 here

                // Check idle state + notify (under the same mutex as the queue)
                {
                    std::lock_guard<std::mutex> lock(m);
                    if (tasks.empty() && activeTasks.load(std::memory_order_relaxed) == 0)
                    {
                        cvIdle.notify_all();
                    }
                }

                // NOTE: no condition.notify_one() here.
                // Producers (enqueue) wake up consumers.
            } });
        }

    public:
        /**
         * @brief Construct a thread pool.
         * @param threadCount    Initial number of worker threads to spawn.
         * @param maxThreadCount Upper bound on worker threads; pool may grow up to this.
         * @param priority       Default priority used by the simpler `enqueue()` overloads.
         * @param maxPeriodic    Maximum concurrently active periodic scheduler threads.
         */
        ThreadPool(std::size_t threadCount,
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

            auto &log = Vix::Logger::getInstance();
            log.log(Vix::Logger::Level::INFO,
                    "[ThreadPool] started with threads={}, maxThreads={}, defaultPrio={}, maxPeriodic={}",
                    threadCount, maxThreads, threadPriority, maxPeriodicThreads);
        }

        /**
         * @brief Snapshot pool metrics (O(1); briefly locks m to read queue size).
         */
        [[nodiscard]] Metrics getMetrics()
        {
            std::lock_guard<std::mutex> lock(m);
            return Metrics{
                static_cast<std::uint64_t>(tasks.size()),     // pending
                activeTasks.load(std::memory_order_relaxed),  // active
                tasksTimedOut.load(std::memory_order_relaxed) // timed_out
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
        auto enqueue(int priority,
                     std::chrono::milliseconds timeout,
                     F &&f, Args &&...args)
            -> std::future<typename std::invoke_result<F, Args...>::type>
        {
            using ReturnType = std::invoke_result<F, Args...>::type;

            // 1. Reject if the pool is stopped
            if (stop.load(std::memory_order_relaxed))
            {
                throw std::runtime_error("ThreadPool is stopped; cannot enqueue new tasks");
            }

            // 2. Prepare the packaged task + future
            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...));

            std::future<ReturnType> res = task->get_future();

            // 3. Arrival number (for FIFO at equal priority)
            const std::uint64_t seq = nextSeq.fetch_add(1, std::memory_order_relaxed);

            {
                std::unique_lock<std::mutex> lock(m);

                // 4. Push into the priority queue (priority, seq) with metric wrapper
                tasks.push(Task{
                    [task, timeout, this]()
                    {
                        const auto start = std::chrono::steady_clock::now();

                        // Not catch here: packaged_task propagates the exception to the future
                        (*task)();

                        if (timeout.count() > 0)
                        {
                            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - start);
                            if (elapsed.count() > timeout.count())
                            {

                                auto &log = Vix::Logger::getInstance();
                                log.log(Vix::Logger::Level::WARN,
                                        "[ThreadPool][Timeout] Thread {} exceeded timeout of {} ms (actual: {} ms)",
                                        threadId, timeout.count(), elapsed.count());
                                tasksTimedOut.fetch_add(1, std::memory_order_relaxed);
                            }
                        }
                    },
                    priority,
                    seq});

                // 5. Elasticity: grow only if saturated + backlog
                const std::uint64_t wcount = workers.size();
                const bool saturated = (activeTasks.load(std::memory_order_relaxed) >= static_cast<std::uint64_t>(wcount));
                const bool backlog = (tasks.size() > wcount);

                if (wcount < maxThreads && saturated && backlog)
                {
                    createThread(wcount); // id = current index
                }
            }

            // 6. Wake up a worker (producer -> consumer)
            condition.notify_one();
            return res;
        }

        /** @overload Enqueue with explicit priority and no timeout. */
        template <class F, class... Args>
        auto enqueue(int priority, F &&f, Args &&...args)
            -> std::future<std::invoke_result_t<F, Args...>>
        {
            return enqueue(priority, std::chrono::milliseconds{0},
                           std::forward<F>(f), std::forward<Args>(args)...);
        }

        /** @overload Enqueue using the pool's default priority. */
        template <class F, class... Args>
        auto enqueue(F &&f, Args &&...args)
            -> std::future<std::invoke_result_t<F, Args...>>
        {
            return enqueue(threadPriority, std::chrono::milliseconds{0},
                           std::forward<F>(f), std::forward<Args>(args)...);
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
            // 1) Obtain a slot without busy-waiting
            {
                std::unique_lock<std::mutex> lock(mPeriodic);
                cvPeriodic.wait(lock, [this]
                                { return stopPeriodic.load(std::memory_order_relaxed) || activePeriodicThreads.load(std::memory_order_relaxed) < maxPeriodicThreads; });

                if (stopPeriodic.load(std::memory_order_relaxed))
                    return; // do not start a new periodic task if stopping

                activePeriodicThreads.fetch_add(1, std::memory_order_relaxed);
            }

            // 2) Start the scheduler thread (rollback safe if creation fails)
            try
            {
                periodicWorkers.emplace_back(
                    [this, priority, func, interval]()
                    {
                        // Thread-local ID for logging
                        threadId = 100000 + static_cast<int>(
                                                std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0x7FFF);

                        auto &log = Vix::Logger::getInstance();

                        // Fixed-rate scheduling: one tick every 'interval'
                        auto next = std::chrono::steady_clock::now() + interval;

                        while (!stopPeriodic.load(std::memory_order_relaxed))
                        {
                            // 2.a Wrap 'func' for logging + rethrow (future keeps the exception)
                            auto wrapped = [func, &log]()
                            {
                                try
                                {
                                    func();
                                }
                                catch (const std::exception &e)
                                {
                                    log.log(Vix::Logger::Level::ERROR,
                                            "[ThreadPool][PeriodicException] Exception in periodic task: {}", e.what());
                                    throw;
                                }
                                catch (...)
                                {
                                    log.log(Vix::Logger::Level::ERROR,
                                            "[ThreadPool][PeriodicException] Unknown exception in periodic task");
                                    throw;
                                }
                            };

                            // 2.b Submit to pool — protect against enqueue() throwing (if pool stopped)
                            std::future<void> future;
                            try
                            {
                                future = enqueue(priority, std::chrono::milliseconds{0}, std::move(wrapped));
                            }
                            catch (const std::exception &e)
                            {
                                log.log(Vix::Logger::Level::WARN,
                                        "[ThreadPool][Periodic] enqueue() failed, stopping scheduler: {}", e.what());
                                break; // exit periodic loop
                            }
                            catch (...)
                            {
                                log.log(Vix::Logger::Level::WARN,
                                        "[ThreadPool][Periodic] enqueue() failed with unknown error, stopping scheduler");
                                break;
                            }

                            // 2.c Wait for next tick OR stop (stopPeriodic) → immediate stop
                            {
                                std::unique_lock<std::mutex> lock(mPeriodic);
                                if (cvPeriodic.wait_until(lock, next, [this]
                                                          { return stopPeriodic.load(std::memory_order_relaxed); }))
                                {
                                    break; // stop requested
                                }
                            }

                            // 2.d Detect interval overrun
                            if (future.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready)
                            {
                                log.log(Vix::Logger::Level::WARN,
                                        "[ThreadPool][PeriodicTimeout] Thread {} periodic task exceeded interval of {} ms",
                                        threadId, interval.count());
                                // do not block; keep fixed-rate schedule
                            }

                            // 2.e Schedule next tick (fixed-rate)
                            next += interval;
                        }

                        // 3) Release slot and notify possible waiters
                        activePeriodicThreads.fetch_sub(1, std::memory_order_relaxed);
                        cvPeriodic.notify_one();
                    });
            }
            catch (...)
            {
                // Rollback slot if thread creation failed
                activePeriodicThreads.fetch_sub(1, std::memory_order_relaxed);
                cvPeriodic.notify_one();
                throw; // rethrow error
            }
        }

        /** @brief True if no task is executing and the queue is empty. */
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

        /** @brief Stop only periodic schedulers; workers remain available. */
        void stopPeriodicTasks() noexcept
        {
            stopPeriodic.store(true, std::memory_order_relaxed);
            condition.notify_all();
        }

        /**
         * @brief Destructor: cooperatively stops and joins all threads.
         *
         * @details
         * - Signals `stop` (workers) and `stopPeriodic` (schedulers).
         * - Wakes all waiters on both condition variables.
         * - Joins periodic threads first, then worker threads.
         * - Allows in-flight tasks to complete (drain).
         */
        ~ThreadPool() noexcept
        {
            // 1) Signal stop (without holding locks for too long)
            {
                std::lock_guard<std::mutex> lk(m);
                stop.store(true, std::memory_order_relaxed);
                stopPeriodic.store(true, std::memory_order_relaxed);
            }

            // 2) Wake everyone up:
            //    - workers waiting on `condition`
            //    - periodic threads waiting on `cvPeriodic`
            condition.notify_all();
            cvPeriodic.notify_all();

            // 3) Join periodic threads first
            for (auto &pworker : periodicWorkers)
            {
                if (pworker.joinable())
                    pworker.join();
            }

            // 4) Then join worker threads
            for (auto &worker : workers)
            {
                if (worker.joinable())
                    worker.join();
            }

            // Note:
            // - No need to notify cvIdle here: if anyone is waiting on waitUntilIdle(),
            //   the transition to idle will be signaled by the worker itself
            //   (after the last task), or the predicate will already be true if idle.
        }
    };

} // namespace Vix

#endif // VIX_THREADPOOL_HPP
