#include <vix/app/App.hpp>
#include <vix/log/Logger.hpp>

#include <cassert>
#include <type_traits>

int main()
{
  static_assert(std::is_same_v<decltype(vix::logger()), vix::log::Logger &>);

  auto &core_logger = vix::logger();
  auto &canonical_logger = vix::log::Logger::getInstance();
  assert(&core_logger == &canonical_logger);

  const auto previous_context = canonical_logger.getContext();
  vix::log::Logger::Context context;
  context.module = "core_logger_identity_test";
  core_logger.setContext(context);

  assert(canonical_logger.getContext().module == context.module);
  canonical_logger.setContext(previous_context);
  return 0;
}
