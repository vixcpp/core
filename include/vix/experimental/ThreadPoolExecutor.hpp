#ifndef VIX_THREAD_POOL_EXECUTOR_HPP
#define VIX_THREAD_POOL_EXECUTOR_HPP

#include <memory>
#include <vix/executor/IExecutor.hpp>
#include <vix/server/ThreadPool.hpp>

#include <vix/executor/TaskOptions.hpp>
#include <vix/executor/Metrics.hpp>

namespace Vix::experimental
{
    class ThreadPoolExecutor final : public Vix::IExecutor
    {
    public:
        explicit ThreadPoolExecutor(std::size_t threads,
                                    std::size_t maxThreads,
                                    int defaultPriority);

        bool post(std::function<void()> fn, Vix::TaskOptions opt = {}) override;
        Vix::executor::Metrics metrics() const override;
        void wait_idle() override;

    private:
        std::unique_ptr<ThreadPool> pool_;
    };

    std::unique_ptr<Vix::IExecutor> make_threadpool_executor(std::size_t threads,
                                                             std::size_t maxThreads,
                                                             int defaultPriority);
}

#endif