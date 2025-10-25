#ifndef VIX_STAND_HPP
#define VIX_STAND_HPP

#include <vix/executor/IExecutor.hpp>

namespace Vix
{
    struct Stand
    {
        IExecutor *underlying{nullptr};
        // TODO: serialiser via une file dediee + mutex
        explicit Stand(IExecutor &ex)
            : underlying(&ex) {}

        bool post(std::function<void()> fn, TaskOptions opt = {})
        {
            return underlying->post(std::move(fn), opt); // stub: sans serialisation
        }
    };

    inline Stand makeStand(IExecutor &ex)
    {
        return Stand(ex);
    }
}

#endif