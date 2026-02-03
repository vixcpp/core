/**
 *
 * @file next.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_NEXT_HPP
#define VIX_NEXT_HPP

#include <functional>
#include <type_traits>
#include <utility>

namespace vix::mw
{

  /** @brief Type alias for the next middleware callable. */
  using NextFn = std::function<void()>;

  /**
   * @brief Middleware continuation callable (call-once).
   *
   * Wraps a callable that can be invoked at most once.
   */
  class Next final
  {
  public:
    /** @brief Construct an empty continuation. */
    Next() = default;

    /** @brief Construct from a NextFn. */
    explicit Next(NextFn fn)
        : fn_(std::move(fn))
    {
    }

    /** @brief Construct from a callable invocable with no arguments. */
    template <class F,
              class = std::enable_if_t<
                  !std::is_same_v<std::decay_t<F>, Next> &&
                  !std::is_same_v<std::decay_t<F>, NextFn> &&
                  std::is_invocable_r_v<void, F>>>
    Next(F &&f)
        : fn_(NextFn(std::forward<F>(f)))
    {
    }

    /** @brief Invoke the continuation if not already called. */
    bool try_call()
    {
      if (called_)
        return false;
      called_ = true;
      if (fn_)
        fn_();
      return true;
    }

    /** @brief Invoke the continuation (no-op if already called). */
    void operator()()
    {
      (void)try_call();
    }

    /** @brief Check whether the continuation was already called. */
    bool called() const noexcept { return called_; }

    /** @brief Check whether a callable is bound. */
    explicit operator bool() const noexcept
    {
      return static_cast<bool>(fn_);
    }

  private:
    NextFn fn_{};
    bool called_{false};
  };

  /** @brief Alias for a call-once continuation. */
  using NextOnce = Next;

} // namespace vix::mw

#endif // VIX_NEXT_HPP
