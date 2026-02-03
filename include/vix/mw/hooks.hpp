/**
 *
 * @file hooks.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_HOOKS_HPP
#define VIX_HOOKS_HPP

#include <functional>
#include <utility>
#include <vector>

#include <vix/mw/context.hpp>
#include <vix/mw/result.hpp>

namespace vix::mw
{

  /**
   * @brief Middleware lifecycle hooks.
   */
  struct Hooks
  {
    std::function<void(Context &)> on_begin{};
    std::function<void(Context &)> on_end{};
    std::function<void(Context &, const Error &)> on_error{};
  };

  /**
   * @brief Merge a list of hooks into a single hook set.
   */
  inline Hooks merge_hooks(std::vector<Hooks> list)
  {
    Hooks out;

    out.on_begin = [list](Context &ctx) mutable
    {
      for (auto &h : list)
      {
        if (h.on_begin)
          h.on_begin(ctx);
      }
    };

    out.on_end = [list](Context &ctx) mutable
    {
      for (std::size_t i = list.size(); i-- > 0;)
      {
        auto &h = list[i];
        if (h.on_end)
          h.on_end(ctx);
      }
    };

    out.on_error = [list](Context &ctx, const Error &err) mutable
    {
      for (std::size_t i = list.size(); i-- > 0;)
      {
        auto &h = list[i];
        if (h.on_error)
          h.on_error(ctx, err);
      }
    };

    return out;
  }

  /**
   * @brief Merge multiple hooks into a single hook set.
   */
  template <class... H>
  inline Hooks merge_hooks(H &&...hooks)
  {
    std::vector<Hooks> list;
    list.reserve(sizeof...(H));
    (list.emplace_back(std::forward<H>(hooks)), ...);
    return merge_hooks(std::move(list));
  }

} // namespace vix::mw

#endif // VIX_HOOKS_HPP
