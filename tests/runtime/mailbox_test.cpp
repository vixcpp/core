/**
 *
 * @file mailbox_test.cpp
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
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

#include <vix/runtime/Mailbox.hpp>

namespace
{
  template <class T>
  using Mailbox = vix::runtime::Mailbox<T>;

  struct Message
  {
    int id{0};
    std::string text{};

    Message() = default;

    Message(int message_id, std::string message_text)
        : id(message_id),
          text(std::move(message_text))
    {
    }

    bool operator==(const Message &other) const
    {
      return id == other.id && text == other.text;
    }
  };

  struct MoveOnlyMessage
  {
    std::unique_ptr<int> value{};

    explicit MoveOnlyMessage(int v)
        : value(std::make_unique<int>(v))
    {
    }

    MoveOnlyMessage(const MoveOnlyMessage &) = delete;
    MoveOnlyMessage &operator=(const MoveOnlyMessage &) = delete;

    MoveOnlyMessage(MoveOnlyMessage &&) noexcept = default;
    MoveOnlyMessage &operator=(MoveOnlyMessage &&) noexcept = default;
  };

  static void test_mailbox_type_traits()
  {
    static_assert(std::is_default_constructible_v<Mailbox<int>>);

    static_assert(!std::is_copy_constructible_v<Mailbox<int>>);
    static_assert(!std::is_copy_assignable_v<Mailbox<int>>);

    static_assert(!std::is_move_constructible_v<Mailbox<int>>);
    static_assert(!std::is_move_assignable_v<Mailbox<int>>);

    static_assert(std::is_destructible_v<Mailbox<int>>);

    static_assert(std::is_same_v<Mailbox<int>::value_type, int>);
    static_assert(std::is_same_v<Mailbox<std::string>::value_type, std::string>);
    static_assert(std::is_same_v<Mailbox<Message>::value_type, Message>);
  }

  static void test_default_mailbox_is_empty()
  {
    Mailbox<int> mailbox;

    assert(mailbox.empty());
    assert(mailbox.size() == 0u);

    auto value = mailbox.try_pop();

    assert(!value.has_value());
    assert(mailbox.empty());
    assert(mailbox.size() == 0u);
  }

  static void test_push_one_value()
  {
    Mailbox<int> mailbox;

    mailbox.push(42);

    assert(!mailbox.empty());
    assert(mailbox.size() == 1u);

    auto value = mailbox.try_pop();

    assert(value.has_value());
    assert(*value == 42);

    assert(mailbox.empty());
    assert(mailbox.size() == 0u);
  }

  static void test_push_preserves_fifo_order()
  {
    Mailbox<int> mailbox;

    mailbox.push(1);
    mailbox.push(2);
    mailbox.push(3);
    mailbox.push(4);

    assert(mailbox.size() == 4u);

    auto first = mailbox.try_pop();
    auto second = mailbox.try_pop();
    auto third = mailbox.try_pop();
    auto fourth = mailbox.try_pop();
    auto fifth = mailbox.try_pop();

    assert(first.has_value());
    assert(second.has_value());
    assert(third.has_value());
    assert(fourth.has_value());
    assert(!fifth.has_value());

    assert(*first == 1);
    assert(*second == 2);
    assert(*third == 3);
    assert(*fourth == 4);

    assert(mailbox.empty());
    assert(mailbox.size() == 0u);
  }

  static void test_push_string_values()
  {
    Mailbox<std::string> mailbox;

    mailbox.push("hello");
    mailbox.push(std::string{"world"});

    assert(mailbox.size() == 2u);

    auto first = mailbox.try_pop();
    auto second = mailbox.try_pop();

    assert(first.has_value());
    assert(second.has_value());

    assert(*first == "hello");
    assert(*second == "world");

    assert(mailbox.empty());
  }

  static void test_emplace_constructs_message_in_place()
  {
    Mailbox<Message> mailbox;

    mailbox.emplace(1, "first");
    mailbox.emplace(2, "second");

    assert(mailbox.size() == 2u);

    auto first = mailbox.try_pop();
    auto second = mailbox.try_pop();

    assert(first.has_value());
    assert(second.has_value());

    assert(first->id == 1);
    assert(first->text == "first");

    assert(second->id == 2);
    assert(second->text == "second");

    assert(mailbox.empty());
  }

  static void test_push_custom_message()
  {
    Mailbox<Message> mailbox;

    mailbox.push(Message{10, "custom"});

    auto value = mailbox.try_pop();

    assert(value.has_value());
    assert(value->id == 10);
    assert(value->text == "custom");

    assert(mailbox.empty());
  }

  static void test_clear_empty_mailbox_is_safe()
  {
    Mailbox<int> mailbox;

    assert(mailbox.empty());
    assert(mailbox.size() == 0u);

    mailbox.clear();
    mailbox.clear();

    assert(mailbox.empty());
    assert(mailbox.size() == 0u);
  }

  static void test_clear_removes_all_messages()
  {
    Mailbox<int> mailbox;

    mailbox.push(1);
    mailbox.push(2);
    mailbox.push(3);

    assert(!mailbox.empty());
    assert(mailbox.size() == 3u);

    mailbox.clear();

    assert(mailbox.empty());
    assert(mailbox.size() == 0u);

    auto value = mailbox.try_pop();

    assert(!value.has_value());
  }

  static void test_clear_then_reuse_mailbox()
  {
    Mailbox<int> mailbox;

    mailbox.push(1);
    mailbox.push(2);

    mailbox.clear();

    assert(mailbox.empty());
    assert(mailbox.size() == 0u);

    mailbox.push(3);
    mailbox.push(4);

    assert(mailbox.size() == 2u);

    auto first = mailbox.try_pop();
    auto second = mailbox.try_pop();
    auto third = mailbox.try_pop();

    assert(first.has_value());
    assert(second.has_value());
    assert(!third.has_value());

    assert(*first == 3);
    assert(*second == 4);

    assert(mailbox.empty());
  }

  static void test_try_pop_until_empty()
  {
    Mailbox<int> mailbox;

    for (int i = 0; i < 10; ++i)
    {
      mailbox.push(i);
    }

    assert(mailbox.size() == 10u);

    for (int i = 0; i < 10; ++i)
    {
      auto value = mailbox.try_pop();

      assert(value.has_value());
      assert(*value == i);
      assert(mailbox.size() == static_cast<std::size_t>(9 - i));
    }

    auto empty = mailbox.try_pop();

    assert(!empty.has_value());
    assert(mailbox.empty());
    assert(mailbox.size() == 0u);
  }

  static void test_size_updates_after_each_operation()
  {
    Mailbox<int> mailbox;

    assert(mailbox.size() == 0u);

    mailbox.push(1);
    assert(mailbox.size() == 1u);

    mailbox.push(2);
    assert(mailbox.size() == 2u);

    mailbox.emplace(3);
    assert(mailbox.size() == 3u);

    auto first = mailbox.try_pop();
    assert(first.has_value());
    assert(mailbox.size() == 2u);

    auto second = mailbox.try_pop();
    assert(second.has_value());
    assert(mailbox.size() == 1u);

    mailbox.clear();
    assert(mailbox.size() == 0u);
    assert(mailbox.empty());
  }

  static void test_empty_updates_after_operations()
  {
    Mailbox<int> mailbox;

    assert(mailbox.empty());

    mailbox.push(1);

    assert(!mailbox.empty());

    auto value = mailbox.try_pop();

    assert(value.has_value());
    assert(mailbox.empty());

    mailbox.emplace(2);

    assert(!mailbox.empty());

    mailbox.clear();

    assert(mailbox.empty());
  }

  static void test_move_only_message_with_push()
  {
    Mailbox<MoveOnlyMessage> mailbox;

    mailbox.push(MoveOnlyMessage{42});

    assert(mailbox.size() == 1u);
    assert(!mailbox.empty());

    auto value = mailbox.try_pop();

    assert(value.has_value());
    assert(value->value != nullptr);
    assert(*value->value == 42);

    assert(mailbox.empty());
  }

  static void test_move_only_message_with_emplace()
  {
    Mailbox<MoveOnlyMessage> mailbox;

    mailbox.emplace(77);

    assert(mailbox.size() == 1u);

    auto value = mailbox.try_pop();

    assert(value.has_value());
    assert(value->value != nullptr);
    assert(*value->value == 77);

    assert(mailbox.empty());
  }

  static void test_optional_is_empty_when_pop_fails()
  {
    Mailbox<std::string> mailbox;

    auto value = mailbox.try_pop();

    assert(!value.has_value());

    mailbox.push("x");

    auto present = mailbox.try_pop();
    auto missing = mailbox.try_pop();

    assert(present.has_value());
    assert(*present == "x");

    assert(!missing.has_value());
  }

  static void test_many_messages_fifo_order()
  {
    Mailbox<int> mailbox;

    constexpr int count = 1000;

    for (int i = 0; i < count; ++i)
    {
      mailbox.push(i);
    }

    assert(mailbox.size() == static_cast<std::size_t>(count));

    for (int i = 0; i < count; ++i)
    {
      auto value = mailbox.try_pop();

      assert(value.has_value());
      assert(*value == i);
    }

    assert(mailbox.empty());
    assert(mailbox.size() == 0u);
  }

  static void test_multiple_producer_threads()
  {
    Mailbox<int> mailbox;

    constexpr int producer_count = 4;
    constexpr int messages_per_producer = 250;
    constexpr int total_messages = producer_count * messages_per_producer;

    std::vector<std::thread> producers;

    for (int p = 0; p < producer_count; ++p)
    {
      producers.emplace_back(
          [&mailbox, p]()
          {
            const int base = p * messages_per_producer;

            for (int i = 0; i < messages_per_producer; ++i)
            {
              mailbox.push(base + i);
            }
          });
    }

    for (auto &thread : producers)
    {
      thread.join();
    }

    assert(mailbox.size() == static_cast<std::size_t>(total_messages));
    assert(!mailbox.empty());

    std::vector<int> values;
    values.reserve(total_messages);

    while (auto value = mailbox.try_pop())
    {
      values.push_back(*value);
    }

    assert(values.size() == static_cast<std::size_t>(total_messages));

    std::sort(values.begin(), values.end());

    for (int i = 0; i < total_messages; ++i)
    {
      assert(values[static_cast<std::size_t>(i)] == i);
    }

    assert(mailbox.empty());
    assert(mailbox.size() == 0u);
  }

  static void test_multiple_consumer_threads()
  {
    Mailbox<int> mailbox;

    constexpr int total_messages = 1000;
    constexpr int consumer_count = 4;

    for (int i = 0; i < total_messages; ++i)
    {
      mailbox.push(i);
    }

    std::vector<int> consumed;
    consumed.reserve(total_messages);

    std::mutex consumed_mutex;
    std::vector<std::thread> consumers;

    for (int c = 0; c < consumer_count; ++c)
    {
      consumers.emplace_back(
          [&mailbox, &consumed, &consumed_mutex]()
          {
            while (true)
            {
              auto value = mailbox.try_pop();

              if (!value.has_value())
              {
                break;
              }

              std::lock_guard<std::mutex> lock(consumed_mutex);
              consumed.push_back(*value);
            }
          });
    }

    for (auto &thread : consumers)
    {
      thread.join();
    }

    assert(consumed.size() == static_cast<std::size_t>(total_messages));

    std::sort(consumed.begin(), consumed.end());

    for (int i = 0; i < total_messages; ++i)
    {
      assert(consumed[static_cast<std::size_t>(i)] == i);
    }

    assert(mailbox.empty());
    assert(mailbox.size() == 0u);
  }

  static void test_concurrent_producers_and_late_consumer()
  {
    Mailbox<int> mailbox;

    constexpr int producer_count = 3;
    constexpr int messages_per_producer = 200;
    constexpr int total_messages = producer_count * messages_per_producer;

    std::vector<std::thread> producers;

    for (int p = 0; p < producer_count; ++p)
    {
      producers.emplace_back(
          [&mailbox, p]()
          {
            const int base = p * messages_per_producer;

            for (int i = 0; i < messages_per_producer; ++i)
            {
              mailbox.emplace(base + i);
            }
          });
    }

    for (auto &thread : producers)
    {
      thread.join();
    }

    std::vector<int> values;
    values.reserve(total_messages);

    while (auto value = mailbox.try_pop())
    {
      values.push_back(*value);
    }

    assert(values.size() == static_cast<std::size_t>(total_messages));

    std::sort(values.begin(), values.end());

    for (int i = 0; i < total_messages; ++i)
    {
      assert(values[static_cast<std::size_t>(i)] == i);
    }

    assert(mailbox.empty());
  }

  static void test_clear_after_concurrent_pushes()
  {
    Mailbox<int> mailbox;

    constexpr int producer_count = 4;
    constexpr int messages_per_producer = 100;

    std::vector<std::thread> producers;

    for (int p = 0; p < producer_count; ++p)
    {
      producers.emplace_back(
          [&mailbox, p]()
          {
            const int base = p * messages_per_producer;

            for (int i = 0; i < messages_per_producer; ++i)
            {
              mailbox.push(base + i);
            }
          });
    }

    for (auto &thread : producers)
    {
      thread.join();
    }

    assert(mailbox.size() == static_cast<std::size_t>(producer_count * messages_per_producer));

    mailbox.clear();

    assert(mailbox.empty());
    assert(mailbox.size() == 0u);
    assert(!mailbox.try_pop().has_value());
  }

} // namespace

int main()
{
  test_mailbox_type_traits();

  test_default_mailbox_is_empty();

  test_push_one_value();
  test_push_preserves_fifo_order();
  test_push_string_values();

  test_emplace_constructs_message_in_place();
  test_push_custom_message();

  test_clear_empty_mailbox_is_safe();
  test_clear_removes_all_messages();
  test_clear_then_reuse_mailbox();

  test_try_pop_until_empty();
  test_size_updates_after_each_operation();
  test_empty_updates_after_operations();

  test_move_only_message_with_push();
  test_move_only_message_with_emplace();

  test_optional_is_empty_when_pop_fails();
  test_many_messages_fifo_order();

  test_multiple_producer_threads();
  test_multiple_consumer_threads();
  test_concurrent_producers_and_late_consumer();
  test_clear_after_concurrent_pushes();

  return 0;
}
