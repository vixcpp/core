#include <cassert>
#include <cstddef>
#include <memory>
#include <span>
#include <system_error>

#include <vix/async/core/cancel.hpp>
#include <vix/async/core/io_context.hpp>
#include <vix/async/core/task.hpp>
#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/router/Router.hpp>
#include <vix/session/Session.hpp>
#include <vix/session/Transport.hpp>

namespace
{
  class FailingReadTransport final : public vix::session::Transport
  {
  public:
    struct State
    {
      bool open{true};
      int close_count{0};
    };

    FailingReadTransport(
        std::errc error,
        std::shared_ptr<State> state)
        : error_(error),
          state_(std::move(state))
    {
    }

    vix::async::core::task<std::size_t> async_read(
        std::span<std::byte>,
        vix::async::core::cancel_token) override
    {
      throw std::system_error(std::make_error_code(error_));
      co_return 0;
    }

    vix::async::core::task<std::size_t> async_write(
        std::span<const std::byte>,
        vix::async::core::cancel_token) override
    {
      co_return 0;
    }

    [[nodiscard]] bool is_open() const noexcept override
    {
      return state_->open;
    }

    void close() noexcept override
    {
      state_->open = false;
      ++state_->close_count;
    }

  private:
    std::errc error_;
    std::shared_ptr<State> state_;
  };

  vix::async::core::task<void> run_session_task(
      vix::async::core::io_context &context,
      std::shared_ptr<vix::session::Session> session)
  {
    co_await session->run();
    context.stop();
    co_return;
  }

  void assert_session_closes_after_read_error(std::errc error)
  {
    vix::router::Router router;
    vix::config::Config config;
    auto executor = std::make_shared<vix::executor::RuntimeExecutor>(1);

    auto state = std::make_shared<FailingReadTransport::State>();
    auto transport = std::make_unique<FailingReadTransport>(error, state);

    auto session = std::make_shared<vix::session::Session>(
        std::move(transport),
        router,
        config,
        executor);

    vix::async::core::io_context context;
    auto task = run_session_task(context, std::move(session));
    std::move(task).start(context.get_scheduler());
    context.run();

    assert(state->close_count >= 1);
    assert(!state->open);

    executor->stop();
  }

  void test_expected_disconnect_closes_session()
  {
    assert_session_closes_after_read_error(std::errc::connection_reset);
  }

  void test_unexpected_error_closes_session()
  {
    assert_session_closes_after_read_error(std::errc::permission_denied);
  }
} // namespace

int main()
{
  test_expected_disconnect_closes_session();
  test_unexpected_error_closes_session();
  return 0;
}
