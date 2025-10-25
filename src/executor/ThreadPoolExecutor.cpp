#include <vix/experimental/ThreadPoolExecutor.hpp>

namespace Vix::experimental
{
    ThreadPoolExecutor::ThreadPoolExecutor(std::size_t threads,
                                           std::size_t maxThreads,
                                           int defaultPriority)
        : pool_(std::make_unique<ThreadPool>(threads, maxThreads, defaultPriority)) {}

    bool ThreadPoolExecutor::post(std::function<void()> fn, Vix::TaskOptions opt)
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

    Vix::executor::Metrics ThreadPoolExecutor::metrics() const
    {
        auto m = pool_->getMetrics();
        return Vix::executor::Metrics{m.pendingTasks, m.activeTasks, m.timedOutTasks};
    }

    void ThreadPoolExecutor::wait_idle()
    {
        pool_->waitUntilIdle();
    }

    std::unique_ptr<Vix::IExecutor> make_threadpool_executor(std::size_t threads, std::size_t maxThreads, int defaultPriority)
    {
        return std::make_unique<ThreadPoolExecutor>(threads, maxThreads, defaultPriority);
    }
}