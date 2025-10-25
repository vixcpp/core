#ifndef VIX_LIMIT_HPP
#define VIX_LIMIT_HPP

#include <vix/executor/IExecutor.hpp>
#include <cstddef>

namespace Vix
{
    struct LimitedExecutor
    {
        IExecutor *underlying{nullptr};

        std::size_t maxPending{32};
        // TODO: compteur atomique + backpressure
        bool post(std::function<void()> fn, TaskOptions opt = {})
        {
            return underlying->post(std::move(fn), opt); // stub
        }
    };

    inline LimitedExecutor limit(IExecutor &ex, std::size_t n)
    {
        return LimitedExecutor{&ex, n};
    }

}

#endif