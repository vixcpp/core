#ifndef VIX_TASK_OPTIONS_HPP
#define VIX_TASK_OPTIONS_HPP

#include <chrono>

namespace vix::executor
{
    struct TaskOptions
    {
        int priority = 0;
        std::chrono::milliseconds timeout{0};
        std::chrono::milliseconds deadline{0}; // 0 => none
        bool may_block = false;
    };
}

#endif