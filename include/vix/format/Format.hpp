/**
 *
 *  @file Format.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira. All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 *  Lightweight formatting utilities for Vix.
 *  This module provides a simple placeholder-based formatting API inspired by
 *  Python and modern formatting libraries, while keeping a small surface area
 *  and minimal complexity.
 *
 *  Supported placeholders:
 *    - {}      : automatic argument indexing
 *    - {0}     : explicit positional indexing
 *    - {{      : escaped opening brace
 *    - }}      : escaped closing brace
 *
 *  Unsupported on purpose:
 *    - format specifiers such as {:>10}, {:.2f}, etc.
 *
 *  Example:
 *
 *    std::string s1 = vix::format("Hello, {}", "world");
 *    std::string s2 = vix::format("Value = {0}, name = {1}", 42, "Ada");
 *    std::string s3 = vix::format("{{ config }} = {}", "ready");
 *
 */

#ifndef VIX_FORMAT_FORMAT_HPP
#define VIX_FORMAT_FORMAT_HPP

#include <array>
#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <sstream>

#include <vix/print.hpp>

namespace vix
{
  /**
   * @brief Exception type thrown on invalid format strings or invalid argument access.
   */
  class format_error : public std::runtime_error
  {
  public:
    /**
     * @brief Construct a formatting error with a message.
     * @param message Human-readable error message.
     */
    explicit format_error(const std::string &message)
        : std::runtime_error(message)
    {
    }

    /**
     * @brief Construct a formatting error with a C-string message.
     * @param message Human-readable error message.
     */
    explicit format_error(const char *message)
        : std::runtime_error(message)
    {
    }
  };

  namespace detail
  {
    /**
     * @brief Convert a value to string using the Vix rendering pipeline.
     * @tparam T Value type.
     * @param value Value to convert.
     * @return String representation of the value.
     */
    template <typename T>
    [[nodiscard]] inline std::string format_arg_to_string(const T &value)
    {
      std::ostringstream oss;

      auto &cfg = vix::default_config();
      const bool old_raw_strings = cfg.raw_strings;
      cfg.raw_strings = false;

      try
      {
        vix::detail::write(oss, value);
      }
      catch (...)
      {
        cfg.raw_strings = old_raw_strings;
        throw;
      }

      cfg.raw_strings = old_raw_strings;
      return oss.str();
    }

    /**
     * @brief Small non-owning view over a pre-rendered argument list.
     */
    class rendered_arg_list
    {
    public:
      /**
       * @brief Construct from a fixed-size array of rendered arguments.
       * @tparam N Number of arguments.
       * @param values Array of argument strings.
       */
      template <std::size_t N>
      explicit rendered_arg_list(const std::array<std::string, N> &values) noexcept
          : data_(values.data()), size_(N)
      {
      }

      /**
       * @brief Return the number of stored arguments.
       * @return Argument count.
       */
      [[nodiscard]] std::size_t size() const noexcept
      {
        return size_;
      }

      /**
       * @brief Get a rendered argument by index.
       * @param index Zero-based argument index.
       * @return Reference to the stored string.
       * @throws vix::format_error If index is out of range.
       */
      [[nodiscard]] const std::string &at(std::size_t index) const
      {
        if (index >= size_)
        {
          throw format_error("format argument index out of range");
        }
        return data_[index];
      }

    private:
      const std::string *data_ = nullptr;
      std::size_t size_ = 0;
    };

    /**
     * @brief Parse an unsigned decimal integer from a placeholder body.
     * @param text Placeholder content without braces.
     * @return Parsed index.
     * @throws vix::format_error If the text is empty or contains non-digits.
     */
    [[nodiscard]] inline std::size_t parse_index(std::string_view text)
    {
      if (text.empty())
      {
        throw format_error("empty explicit format index");
      }

      std::size_t value = 0;
      for (char ch : text)
      {
        if (!std::isdigit(static_cast<unsigned char>(ch)))
        {
          throw format_error("invalid explicit format index");
        }

        value = (value * 10u) + static_cast<std::size_t>(ch - '0');
      }

      return value;
    }

    /**
     * @brief Render a format string into the destination string.
     *
     * Rules:
     *   - {} inserts the next automatic argument.
     *   - {N} inserts argument N.
     *   - {{ inserts '{'
     *   - }} inserts '}'
     *
     * Mixing automatic indexing ({}) with explicit indexing ({0}) is rejected.
     *
     * @param out Destination string.
     * @param fmt Format string.
     * @param args Pre-rendered argument list.
     * @throws vix::format_error On malformed input.
     */
    inline void render_format_string(std::string &out,
                                     std::string_view fmt,
                                     const rendered_arg_list &args)
    {
      std::size_t i = 0;
      std::size_t next_auto_index = 0;
      bool used_auto_index = false;
      bool used_explicit_index = false;

      while (i < fmt.size())
      {
        const char ch = fmt[i];

        if (ch == '{')
        {
          if ((i + 1u) < fmt.size() && fmt[i + 1u] == '{')
          {
            out.push_back('{');
            i += 2u;
            continue;
          }

          const std::size_t close = fmt.find('}', i + 1u);
          if (close == std::string_view::npos)
          {
            throw format_error("unmatched '{' in format string");
          }

          const std::string_view token = fmt.substr(i + 1u, close - (i + 1u));

          if (token.empty())
          {
            if (used_explicit_index)
            {
              throw format_error("cannot mix automatic and explicit argument indexing");
            }

            used_auto_index = true;
            out += args.at(next_auto_index++);
          }
          else
          {
            if (used_auto_index)
            {
              throw format_error("cannot mix automatic and explicit argument indexing");
            }

            used_explicit_index = true;

            if (token.find(':') != std::string_view::npos)
            {
              throw format_error("format specifiers are not supported");
            }

            const std::size_t index = parse_index(token);
            out += args.at(index);
          }

          i = close + 1u;
          continue;
        }

        if (ch == '}')
        {
          if ((i + 1u) < fmt.size() && fmt[i + 1u] == '}')
          {
            out.push_back('}');
            i += 2u;
            continue;
          }

          throw format_error("single '}' encountered in format string");
        }

        out.push_back(ch);
        ++i;
      }
    }

    /**
     * @brief Estimate a useful output capacity before rendering.
     * @param fmt Format string.
     * @param args Pre-rendered argument list.
     * @return Estimated output size.
     */
    [[nodiscard]] inline std::size_t estimate_output_size(std::string_view fmt,
                                                          const rendered_arg_list &args) noexcept
    {
      std::size_t total = fmt.size();
      for (std::size_t i = 0; i < args.size(); ++i)
      {
        total += args.at(i).size();
      }
      return total;
    }

  } // namespace detail

  /**
   * @brief Format values into a new string using Vix placeholder syntax.
   *
   * Supported placeholders:
   *   - {}  automatic indexing
   *   - {0} explicit indexing
   *   - {{  escaped '{'
   *   - }}  escaped '}'
   *
   * @tparam Args Argument types.
   * @param fmt Format string.
   * @param args Values to inject.
   * @return Newly formatted string.
   *
   * @throws vix::format_error If the format string is malformed or references
   *         a missing argument.
   *
   * @example
   *   auto s = vix::format("Hello, {}", "world");
   *   auto t = vix::format("{0} + {0} = {1}", 2, 4);
   */
  template <typename... Args>
  [[nodiscard]] std::string format(std::string_view fmt, const Args &...args)
  {
    std::array<std::string, sizeof...(Args)> rendered_args{
        detail::format_arg_to_string(args)...};

    detail::rendered_arg_list rendered{rendered_args};

    std::string out;
    out.reserve(detail::estimate_output_size(fmt, rendered));
    detail::render_format_string(out, fmt, rendered);
    return out;
  }

  /**
   * @brief Append formatted output to an existing string.
   *
   * This avoids replacing the content of the destination and is useful for
   * incremental string building.
   *
   * @tparam Args Argument types.
   * @param out Destination string.
   * @param fmt Format string.
   * @param args Values to inject.
   *
   * @throws vix::format_error If the format string is malformed or references
   *         a missing argument.
   */
  template <typename... Args>
  void format_append(std::string &out, std::string_view fmt, const Args &...args)
  {
    std::array<std::string, sizeof...(Args)> rendered_args{
        detail::format_arg_to_string(args)...};

    detail::rendered_arg_list rendered{rendered_args};
    out.reserve(out.size() + detail::estimate_output_size(fmt, rendered));
    detail::render_format_string(out, fmt, rendered);
  }

  /**
   * @brief Replace the destination string with formatted output.
   *
   * @tparam Args Argument types.
   * @param out Destination string.
   * @param fmt Format string.
   * @param args Values to inject.
   *
   * @throws vix::format_error If the format string is malformed or references
   *         a missing argument.
   */
  template <typename... Args>
  void format_to(std::string &out, std::string_view fmt, const Args &...args)
  {
    out.clear();
    format_append(out, fmt, args...);
  }

} // namespace vix

#endif // VIX_FORMAT_FORMAT_HPP
