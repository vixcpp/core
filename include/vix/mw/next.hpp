/**
 *
 *  @file next.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 */
#ifndef VIX_NEXT_HPP
#define VIX_NEXT_HPP

#include <functional>
#include <type_traits>
#include <utility>

namespace vix::mw
{
  using NextFn = std::function<void()>;

  class Next final
  {
  public:
    Next() = default;

    explicit Next(NextFn fn)
        : fn_(std::move(fn))
    {
    }

    template <class F,
              class = std::enable_if_t<
                  !std::is_same_v<std::decay_t<F>, Next> &&
                  !std::is_same_v<std::decay_t<F>, NextFn> &&
                  std::is_invocable_r_v<void, F>>>
    Next(F &&f)
        : fn_(NextFn(std::forward<F>(f)))
    {
    }

    bool try_call()
    {
      if (called_)
        return false;
      called_ = true;
      if (fn_)
        fn_();
      return true;
    }

    void operator()()
    {
      (void)try_call();
    }

    bool called() const noexcept { return called_; }

    explicit operator bool() const noexcept
    {
      return static_cast<bool>(fn_);
    }

  private:
    NextFn fn_{};
    bool called_{false};
  };

  using NextOnce = Next;

} // namespace vix::mw

#endif
