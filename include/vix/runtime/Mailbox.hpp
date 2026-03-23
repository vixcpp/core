/**
 *
 * @file Mailbox.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the
 * License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_RUNTIME_MAILBOX_HPP
#define VIX_RUNTIME_MAILBOX_HPP

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <type_traits>
#include <utility>

namespace vix::runtime
{

  /**
   * @brief Thread-safe FIFO mailbox for lightweight message passing.
   *
   * Mailbox is intentionally simple in V1:
   * - multiple producers are allowed
   * - one or more consumers may pop safely
   * - messages are stored in FIFO order
   *
   * This mailbox does not block.
   * Users call @ref push and @ref try_pop explicitly.
   *
   * @tparam T Message type stored in the mailbox.
   */
  template <class T>
  class Mailbox
  {
    static_assert(!std::is_reference_v<T>,
                  "Mailbox<T> does not support reference message types");

  public:
    /** @brief Message value type. */
    using value_type = T;

    /**
     * @brief Construct an empty mailbox.
     */
    Mailbox() = default;

    Mailbox(const Mailbox &) = delete;
    Mailbox &operator=(const Mailbox &) = delete;

    Mailbox(Mailbox &&) = delete;
    Mailbox &operator=(Mailbox &&) = delete;

    /**
     * @brief Push one message into the mailbox.
     *
     * @param value Message to enqueue.
     */
    void push(T value)
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.push_back(std::move(value));
    }

    /**
     * @brief Construct and push one message in-place.
     *
     * @tparam Args Constructor argument types.
     * @param args Constructor arguments forwarded to T.
     */
    template <class... Args>
    void emplace(Args &&...args)
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.emplace_back(std::forward<Args>(args)...);
    }

    /**
     * @brief Try to pop one message from the mailbox.
     *
     * @return The next message if one exists, std::nullopt otherwise.
     */
    [[nodiscard]] std::optional<T> try_pop()
    {
      std::lock_guard<std::mutex> lock(mutex_);

      if (queue_.empty())
      {
        return std::nullopt;
      }

      T value = std::move(queue_.front());
      queue_.pop_front();
      return value;
    }

    /**
     * @brief Return whether the mailbox is empty.
     *
     * @return true if there is no pending message.
     */
    [[nodiscard]] bool empty() const
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return queue_.empty();
    }

    /**
     * @brief Return the number of pending messages.
     *
     * @return Current mailbox size.
     */
    [[nodiscard]] std::size_t size() const
    {
      std::lock_guard<std::mutex> lock(mutex_);
      return queue_.size();
    }

    /**
     * @brief Remove all pending messages.
     */
    void clear()
    {
      std::lock_guard<std::mutex> lock(mutex_);
      queue_.clear();
    }

  private:
    /** @brief FIFO storage for pending messages. */
    std::deque<T> queue_;

    /** @brief Mutex protecting mailbox access. */
    mutable std::mutex mutex_;
  };

} // namespace vix::runtime

#endif // VIX_RUNTIME_MAILBOX_HPP
