#ifndef VIX_EXECUTOR_RUNTIME_EXECUTOR_HPP
#define VIX_EXECUTOR_RUNTIME_EXECUTOR_HPP

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <utility>

#include <vix/executor/Metrics.hpp>
#include <vix/executor/TaskOptions.hpp>
#include <vix/runtime/Runtime.hpp>
#include <vix/runtime/Task.hpp>

namespace vix::executor
{
  class RuntimeExecutor final
  {
  public:
    explicit RuntimeExecutor(
        const vix::runtime::RuntimeConfig &config = vix::runtime::RuntimeConfig{})
        : runtime_(std::make_unique<vix::runtime::Runtime>(config)),
          active_(0),
          timed_out_(0),
          started_(false)
    {
    }

    explicit RuntimeExecutor(std::unique_ptr<vix::runtime::Runtime> runtime)
        : runtime_(std::move(runtime)),
          active_(0),
          timed_out_(0),
          started_(false)
    {
      if (!runtime_)
      {
        throw std::invalid_argument("RuntimeExecutor requires a valid runtime");
      }
    }

    ~RuntimeExecutor()
    {
      stop();
    }

    RuntimeExecutor(const RuntimeExecutor &) = delete;
    RuntimeExecutor &operator=(const RuntimeExecutor &) = delete;

    RuntimeExecutor(RuntimeExecutor &&) = delete;
    RuntimeExecutor &operator=(RuntimeExecutor &&) = delete;

    void start()
    {
      bool expected = false;
      if (started_.compare_exchange_strong(expected,
                                           true,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire))
      {
        runtime_->start();
      }
    }

    void stop()
    {
      bool expected = true;
      if (started_.compare_exchange_strong(expected,
                                           false,
                                           std::memory_order_acq_rel,
                                           std::memory_order_acquire))
      {
        runtime_->stop();
      }
    }

    [[nodiscard]] bool post(std::function<void()> fn,
                            TaskOptions opt = {})
    {
      if (!fn)
      {
        return false;
      }

      (void)opt;

      return runtime_->submit(
          [this, task = std::move(fn)]() mutable -> vix::runtime::TaskResult
          {
            active_.fetch_add(1, std::memory_order_relaxed);

            try
            {
              task();
              active_.fetch_sub(1, std::memory_order_relaxed);
              return vix::runtime::TaskResult::complete;
            }
            catch (...)
            {
              active_.fetch_sub(1, std::memory_order_relaxed);
              return vix::runtime::TaskResult::failed;
            }
          });
    }

    [[nodiscard]] vix::executor::Metrics metrics() const
    {
      vix::executor::Metrics m;
      m.pending = static_cast<std::uint64_t>(runtime_->size());
      m.active = active_.load(std::memory_order_relaxed);
      m.timed_out = timed_out_.load(std::memory_order_relaxed);
      return m;
    }

    void wait_idle() const
    {
      for (;;)
      {
        const auto m = metrics();
        if (m.pending == 0 && m.active == 0)
        {
          break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
    }

    [[nodiscard]] vix::runtime::Runtime &runtime() noexcept
    {
      return *runtime_;
    }

    [[nodiscard]] const vix::runtime::Runtime &runtime() const noexcept
    {
      return *runtime_;
    }

  private:
    std::unique_ptr<vix::runtime::Runtime> runtime_;
    std::atomic<std::uint64_t> active_;
    std::atomic<std::uint64_t> timed_out_;
    std::atomic<bool> started_;
  };

} // namespace vix::executor

#endif // VIX_EXECUTOR_RUNTIME_EXECUTOR_HPP
