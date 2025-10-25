#ifndef VIX_IEXECUTOR_HPP
#define VIX_IEXECUTOR_HPP

#include <future>
#include <functional>
#include <type_traits>
#include <memory>

#include <vix/executor/TaskOptions.hpp>
#include <vix/executor/Metrics.hpp>

namespace Vix
{
    struct IExecutor
    {
        virtual ~IExecutor() = default;

        virtual bool post(std::function<void()> fn, TaskOptions opt = {}) = 0;
        virtual Metrics metrics() const = 0;
        virtual void wait_idle() = 0;

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
}

#endif