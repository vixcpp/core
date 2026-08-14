/**
 * @file RuntimeExecutor.cpp
 * @brief Out-of-line lifecycle definitions for RuntimeExecutor.
 */

#include <vix/executor/RuntimeExecutor.hpp>

namespace vix::executor
{
  RuntimeExecutor::RuntimeExecutor(const vix::runtime::RuntimeConfig &config)
      : runtime_(std::make_unique<vix::runtime::Runtime>(config)),
        state_(std::make_shared<SharedState>()),
        started_(false)
  {
  }

  RuntimeExecutor::RuntimeExecutor(std::uint32_t workers)
      : RuntimeExecutor(make_config_from_workers(workers))
  {
  }

  RuntimeExecutor::RuntimeExecutor(std::unique_ptr<vix::runtime::Runtime> runtime)
      : runtime_(std::move(runtime)),
        state_(std::make_shared<SharedState>()),
        started_(false)
  {
    if (!runtime_)
    {
      throw std::invalid_argument("RuntimeExecutor requires a valid runtime");
    }
  }

  RuntimeExecutor::~RuntimeExecutor() noexcept
  {
    try
    {
      stop();
    }
    catch (...)
    {
    }
  }
} // namespace vix::executor
