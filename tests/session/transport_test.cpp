/**
 *
 * @file transport_test.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <vix/async/core/cancel.hpp>
#include <vix/async/core/io_context.hpp>
#include <vix/async/core/task.hpp>
#include <vix/session/Transport.hpp>

namespace
{
  class FakeTransport final : public vix::session::Transport
  {
  public:
    explicit FakeTransport(std::string input = {})
        : input_(std::move(input))
    {
    }

    vix::async::core::task<std::size_t> async_read(
        std::span<std::byte> buffer,
        vix::async::core::cancel_token token) override
    {
      if (token.is_cancelled())
      {
        co_return 0;
      }

      if (!open_)
      {
        co_return 0;
      }

      if (buffer.empty())
      {
        co_return 0;
      }

      if (read_offset_ >= input_.size())
      {
        co_return 0;
      }

      const std::size_t remaining = input_.size() - read_offset_;
      const std::size_t n = std::min(buffer.size(), remaining);

      std::memcpy(
          buffer.data(),
          input_.data() + read_offset_,
          n);

      read_offset_ += n;

      co_return n;
    }

    vix::async::core::task<std::size_t> async_write(
        std::span<const std::byte> buffer,
        vix::async::core::cancel_token token) override
    {
      if (token.is_cancelled())
      {
        co_return 0;
      }

      if (!open_)
      {
        co_return 0;
      }

      if (buffer.empty())
      {
        co_return 0;
      }

      const auto *data = reinterpret_cast<const char *>(buffer.data());

      output_.append(data, buffer.size());

      co_return buffer.size();
    }

    [[nodiscard]] bool is_open() const noexcept override
    {
      return open_;
    }

    void close() noexcept override
    {
      open_ = false;
    }

    [[nodiscard]] const std::string &output() const noexcept
    {
      return output_;
    }

    [[nodiscard]] std::size_t read_offset() const noexcept
    {
      return read_offset_;
    }

  private:
    std::string input_{};
    std::string output_{};
    std::size_t read_offset_{0};
    bool open_{true};
  };

  static vix::async::core::task<void> read_once_task(
      vix::async::core::io_context &ctx,
      vix::session::Transport &transport,
      std::span<std::byte> buffer,
      std::size_t &bytes_read)
  {
    bytes_read = co_await transport.async_read(buffer, {});
    ctx.stop();
    co_return;
  }

  static vix::async::core::task<void> write_once_task(
      vix::async::core::io_context &ctx,
      vix::session::Transport &transport,
      std::span<const std::byte> buffer,
      std::size_t &bytes_written)
  {
    bytes_written = co_await transport.async_write(buffer, {});
    ctx.stop();
    co_return;
  }

  static std::size_t run_read_once(
      vix::session::Transport &transport,
      std::span<std::byte> buffer)
  {
    vix::async::core::io_context ctx;

    std::size_t bytes_read = 0;

    auto task = read_once_task(
        ctx,
        transport,
        buffer,
        bytes_read);

    std::move(task).start(ctx.get_scheduler());

    ctx.run();

    return bytes_read;
  }

  static std::size_t run_write_once(
      vix::session::Transport &transport,
      std::span<const std::byte> buffer)
  {
    vix::async::core::io_context ctx;

    std::size_t bytes_written = 0;

    auto task = write_once_task(
        ctx,
        transport,
        buffer,
        bytes_written);

    std::move(task).start(ctx.get_scheduler());

    ctx.run();

    return bytes_written;
  }

  static std::span<const std::byte> bytes_view(std::string_view text)
  {
    return std::span<const std::byte>(
        reinterpret_cast<const std::byte *>(text.data()),
        text.size());
  }

  static std::string buffer_to_string(
      const std::vector<std::byte> &buffer,
      std::size_t n)
  {
    return std::string(
        reinterpret_cast<const char *>(buffer.data()),
        n);
  }

  static void test_transport_type_traits()
  {
    static_assert(std::has_virtual_destructor_v<vix::session::Transport>);
    static_assert(std::is_abstract_v<vix::session::Transport>);

    static_assert(!std::is_copy_constructible_v<vix::session::Transport>);
    static_assert(!std::is_copy_assignable_v<vix::session::Transport>);

    static_assert(!std::is_move_constructible_v<vix::session::Transport>);
    static_assert(!std::is_move_assignable_v<vix::session::Transport>);
  }

  static void test_fake_transport_starts_open()
  {
    FakeTransport transport;

    assert(transport.is_open());
    assert(transport.output().empty());
    assert(transport.read_offset() == 0);
  }

  static void test_close_is_idempotent()
  {
    FakeTransport transport;

    assert(transport.is_open());

    transport.close();

    assert(!transport.is_open());

    transport.close();
    transport.close();

    assert(!transport.is_open());
  }

  static void test_read_full_payload()
  {
    FakeTransport transport{"hello"};

    std::vector<std::byte> buffer(16);

    const std::size_t n = run_read_once(
        transport,
        std::span<std::byte>(buffer.data(), buffer.size()));

    assert(n == 5);
    assert(buffer_to_string(buffer, n) == "hello");
    assert(transport.read_offset() == 5);
  }

  static void test_read_partial_payload()
  {
    FakeTransport transport{"hello world"};

    std::vector<std::byte> buffer(5);

    const std::size_t n = run_read_once(
        transport,
        std::span<std::byte>(buffer.data(), buffer.size()));

    assert(n == 5);
    assert(buffer_to_string(buffer, n) == "hello");
    assert(transport.read_offset() == 5);
  }

  static void test_read_multiple_chunks()
  {
    FakeTransport transport{"abcdef"};

    std::vector<std::byte> first(2);
    std::vector<std::byte> second(3);
    std::vector<std::byte> third(8);

    const std::size_t n1 = run_read_once(
        transport,
        std::span<std::byte>(first.data(), first.size()));

    const std::size_t n2 = run_read_once(
        transport,
        std::span<std::byte>(second.data(), second.size()));

    const std::size_t n3 = run_read_once(
        transport,
        std::span<std::byte>(third.data(), third.size()));

    assert(n1 == 2);
    assert(n2 == 3);
    assert(n3 == 1);

    assert(buffer_to_string(first, n1) == "ab");
    assert(buffer_to_string(second, n2) == "cde");
    assert(buffer_to_string(third, n3) == "f");

    assert(transport.read_offset() == 6);
  }

  static void test_read_returns_zero_at_eof()
  {
    FakeTransport transport{"x"};

    std::vector<std::byte> buffer(8);

    const std::size_t n1 = run_read_once(
        transport,
        std::span<std::byte>(buffer.data(), buffer.size()));

    const std::size_t n2 = run_read_once(
        transport,
        std::span<std::byte>(buffer.data(), buffer.size()));

    assert(n1 == 1);
    assert(n2 == 0);
  }

  static void test_read_empty_buffer_returns_zero()
  {
    FakeTransport transport{"hello"};

    std::vector<std::byte> buffer;

    const std::size_t n = run_read_once(
        transport,
        std::span<std::byte>(buffer.data(), buffer.size()));

    assert(n == 0);
    assert(transport.read_offset() == 0);
  }

  static void test_read_closed_transport_returns_zero()
  {
    FakeTransport transport{"hello"};

    transport.close();

    std::vector<std::byte> buffer(16);

    const std::size_t n = run_read_once(
        transport,
        std::span<std::byte>(buffer.data(), buffer.size()));

    assert(n == 0);
    assert(transport.read_offset() == 0);
  }

  static void test_write_full_payload()
  {
    FakeTransport transport;

    const std::string payload = "hello";

    const std::size_t n = run_write_once(
        transport,
        bytes_view(payload));

    assert(n == payload.size());
    assert(transport.output() == "hello");
  }

  static void test_write_multiple_payloads()
  {
    FakeTransport transport;

    const std::size_t n1 = run_write_once(
        transport,
        bytes_view("hello"));

    const std::size_t n2 = run_write_once(
        transport,
        bytes_view(" "));

    const std::size_t n3 = run_write_once(
        transport,
        bytes_view("world"));

    assert(n1 == 5);
    assert(n2 == 1);
    assert(n3 == 5);

    assert(transport.output() == "hello world");
  }

  static void test_write_empty_buffer_returns_zero()
  {
    FakeTransport transport;

    const std::size_t n = run_write_once(
        transport,
        bytes_view(""));

    assert(n == 0);
    assert(transport.output().empty());
  }

  static void test_write_closed_transport_returns_zero()
  {
    FakeTransport transport;

    transport.close();

    const std::size_t n = run_write_once(
        transport,
        bytes_view("hello"));

    assert(n == 0);
    assert(transport.output().empty());
  }

  static void test_transport_can_be_used_through_base_reference()
  {
    FakeTransport fake{"abc"};

    vix::session::Transport &transport = fake;

    std::vector<std::byte> buffer(3);

    const std::size_t read_n = run_read_once(
        transport,
        std::span<std::byte>(buffer.data(), buffer.size()));

    assert(read_n == 3);
    assert(buffer_to_string(buffer, read_n) == "abc");

    const std::size_t write_n = run_write_once(
        transport,
        bytes_view("xyz"));

    assert(write_n == 3);
    assert(fake.output() == "xyz");

    assert(transport.is_open());

    transport.close();

    assert(!transport.is_open());
  }

} // namespace

int main()
{
  test_transport_type_traits();
  test_fake_transport_starts_open();
  test_close_is_idempotent();

  test_read_full_payload();
  test_read_partial_payload();
  test_read_multiple_chunks();
  test_read_returns_zero_at_eof();
  test_read_empty_buffer_returns_zero();
  test_read_closed_transport_returns_zero();

  test_write_full_payload();
  test_write_multiple_payloads();
  test_write_empty_buffer_returns_zero();
  test_write_closed_transport_returns_zero();

  test_transport_can_be_used_through_base_reference();

  return 0;
}
