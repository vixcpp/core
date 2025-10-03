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
#include <utility>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <pthread.h>
#include <unordered_map>
#include <shared_mutex>

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
            return priority < other.priority;
        }
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
        std::unordered_map<std::thread::id, int> threadAffinity;
        std::atomic<int> activeTasks;

        int threadPriority;

        void setThreadAffinity(int id)
        {
#ifdef __linux__
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(id % std::thread::hardware_concurrency(), &cpuset);
            pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
#endif
        }

    public:
        ThreadPool(size_t threadCount, size_t maxThreadCount, int priority, std::chrono::milliseconds)
            : workers(),
              tasks(),
              stop(false),
              stopPeriodic(false),
              maxThreads(maxThreadCount),
              threadAffinity(),
              activeTasks(0),
              threadPriority(priority)
        {
            for (size_t i = 0; i < threadCount; ++i)
                createThread(i);
        }

        void createThread(int id)
        {
            workers.emplace_back([this, id]
                                 {
                threadId = id;
                threadAffinity[std::this_thread::get_id()] = id;
    
                setThreadAffinity(id);  
    
                while (true) {
                    Task task;
                    {
                        std::unique_lock<std::mutex> lock(m);
                        condition.wait(lock, [this] { return stop || !tasks.empty(); });
    
                        if (stop && tasks.empty()) return;
    
                        task = std::move(tasks.top());
                        tasks.pop();
                        ++activeTasks;
                    }
                    try {
                        task.func();
                    } catch (const std::exception& e) {
                        std::cerr << "Exception in thread " << threadId << ": " << e.what() << std::endl;
                    }
                    --activeTasks;
                    condition.notify_one();
                } });
        }

        template <class F, class... Args>
        auto enqueue(int priority, F &&f, Args &&...args)
            -> std::future<typename std::invoke_result<F, Args...>::type>
        {
            using ReturnType = typename std::invoke_result<F, Args...>::type;
            auto task = std::make_shared<std::packaged_task<ReturnType()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...));
            std::future<ReturnType> res = task->get_future();
            {
                std::unique_lock<std::mutex> lock(m);
                tasks.push(Vix::Task{
                    [task]()
                    {
                        try
                        {
                            (*task)();
                        }
                        catch (const std::exception &e)
                        {
                            std::cerr << "An exception occurred during task execution: " << e.what() << std::endl;
                        }
                    },
                    priority});

                if (workers.size() < maxThreads)
                {
                    createThread(workers.size());
                }
            }
            condition.notify_one();
            return res;
        }

        template <class F, class... Args>
        auto enqueue(F &&f, Args &&...args)
            -> std::future<typename std::invoke_result<F, Args...>::type>
        {
            return enqueue(threadPriority, std::forward<F>(f), std::forward<Args>(args)...);
        }

        void periodicTask(int priority, std::function<void()> func, std::chrono::milliseconds interval)
        {
            auto loop = [this, priority, func, interval]()
            {
                while (!stopPeriodic)
                {
                    try
                    {
                        auto future = enqueue(priority, func);
                        if (future.wait_for(interval) == std::future_status::timeout)
                        {
                            std::cerr << "Periodic task cancelled due to timeout." << std::endl;
                        }
                    }
                    catch (const std::exception &e)
                    {
                        std::cerr << "Exception in periodic task: " << e.what() << std::endl;
                    }

                    std::this_thread::sleep_for(interval);
                }
            };

            std::thread(loop).detach();
        }

        bool isIdle()
        {
            return activeTasks.load() == 0 && tasks.empty();
        }

        void stopPeriodicTasks()
        {
            stopPeriodic = true;
        }

        ~ThreadPool()
        {
            {
                std::unique_lock<std::mutex> lock(m);
                stop = true;
                stopPeriodic = true;
            }
            condition.notify_all();
            for (std::thread &worker : workers)
            {
                if (worker.joinable())
                {
                    worker.join();
                }
            }
        }
    };

}

#endif
