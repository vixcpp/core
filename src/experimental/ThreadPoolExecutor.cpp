#include <vix/experimental/ThreadPoolExecutor.hpp>

namespace vix::experimental
{
    ThreadPoolExecutor::ThreadPoolExecutor(std::size_t threads,
                                           std::size_t maxThreads,
                                           int defaultPriority)
        : pool_(std::make_unique<vix::threadpool::ThreadPool>(threads, maxThreads, defaultPriority)) {}

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
