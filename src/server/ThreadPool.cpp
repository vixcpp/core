#include <vix/server/ThreadPool.hpp>

/**
 * @file ThreadPool.cpp
 * @brief Defines the global thread-local identifier for worker threads.
 *
 * @details
 * Each worker thread spawned by `Vix::ThreadPool` assigns a unique, stable
 * integer ID to `threadId`. This identifier is used exclusively for logging
 * and debugging purposes (e.g., to prefix log messages with `[Thread N]`).
 *
 * The value is:
 *   - Initialized to `-1` in threads that are not part of the pool.
 *   - Set by `ThreadPool::createThread()` when a worker starts running.
 *
 * @note
 *  - This file intentionally contains no logic beyond the definition of the
 *    `thread_local` variable; it must be linked exactly once in the build.
 *  - External modules (Logger, Scheduler, etc.) may read but should not
 *    modify `Vix::threadId` directly.
 */

namespace Vix
{
    /// Thread-local ID assigned to each worker for logging and metrics.
    thread_local int threadId = -1;
}
