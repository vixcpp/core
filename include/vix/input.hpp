/**
 *
 *  @file input.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2026, Gaspard Kirira.
 *  All rights reserved.
 *  https://github.com/vixcpp/vix
 *
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 *  Vix Input (core) - Simple interactive input for console apps
 */
#ifndef VIX_INPUT_HPP
#define VIX_INPUT_HPP

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace vix
{
  namespace detail
  {
    /**
     * @brief Print a prompt immediately without appending a newline.
     *
     * This intentionally uses std::cout directly so interactive prompts remain
     * predictable and do not depend on higher-level formatting layers.
     *
     * @param prompt Prompt text.
     *
     * @throws std::runtime_error if writing or flushing the prompt fails.
     */
    inline void print_prompt(std::string_view prompt)
    {
      if (prompt.empty())
      {
        return;
      }

      std::cout.write(prompt.data(),
                      static_cast<std::streamsize>(prompt.size()));

      if (!std::cout)
      {
        throw std::runtime_error(
            "vix::input failed: could not write prompt to output stream");
      }

      std::cout.flush();

      if (!std::cout)
      {
        throw std::runtime_error(
            "vix::input failed: could not flush prompt to output stream");
      }
    }

    /**
     * @brief Read one full line from std::cin with robust error handling.
     *
     * @return The captured line without the trailing newline.
     *
     * @throws std::runtime_error if the input stream is closed or fails.
     */
    [[nodiscard]] inline std::string read_line()
    {
      std::string line;

      if (std::getline(std::cin, line))
      {
        if (!line.empty() && line.back() == '\r')
        {
          line.pop_back();
        }

        return line;
      }

      if (std::cin.eof())
      {
        throw std::runtime_error(
            "vix::input failed: input stream is closed");
      }

      std::cin.clear();

      throw std::runtime_error(
          "vix::input failed: could not read from input stream");
    }
  } // namespace detail

  /**
   * @brief Read a full line from standard input.
   *
   * This is the single public input API in Vix.
   * It reads one complete line and returns it as a std::string.
   *
   * @return User input line.
   *
   * @throws std::runtime_error if reading from the input stream fails.
   *
   * @example
   *   auto name = vix::input();
   */
  [[nodiscard]] inline std::string input()
  {
    return detail::read_line();
  }

  /**
   * @brief Print a prompt, then read a full line from standard input.
   *
   * The prompt is printed without a trailing newline, similar to Python's
   * input().
   *
   * @param prompt Prompt text displayed before reading.
   * @return User input line.
   *
   * @throws std::runtime_error if writing the prompt fails or reading input
   *         fails.
   *
   * @example
   *   auto name = vix::input("Enter your name: ");
   */
  [[nodiscard]] inline std::string input(std::string_view prompt)
  {
    detail::print_prompt(prompt);
    return detail::read_line();
  }

} // namespace vix

#endif // VIX_INPUT_HPP
