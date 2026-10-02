/**
 * @file ServerReadyPresentation.hpp
 * @brief HTTP-server readiness data and startup presentation.
 */
#ifndef VIX_SERVER_READY_PRESENTATION_HPP
#define VIX_SERVER_READY_PRESENTATION_HPP

#include <chrono>
#include <cctype>
#include <cstddef>
#include <cstdio>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

#include <vix/env/GetOr.hpp>
#include <vix/env/Has.hpp>
#include <vix/log/ConsoleSync.hpp>

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace vix::server
{
  /**
   * @brief Data rendered when an HTTP server has successfully started.
   *
   * This is HTTP/server startup presentation data. It is intentionally not a
   * Runtime Foundation-wide readiness model.
   */
  struct ServerReadyInfo
  {
    std::string app = "vix.cpp";
    std::string version;
    int ready_ms = -1;
    std::string mode;
    std::string status = "ready";
    std::string config_path;
    std::string host = "localhost";
    int port = 8080;
    std::string scheme = "http";
    std::string base_path = "/";
    bool show_ws = true;
    int ws_port = 9090;
    std::string ws_scheme = "ws";
    std::string ws_host = "localhost";
    std::string ws_path = "/";
    bool show_hints = true;
    std::size_t threads = 0;
    std::size_t max_threads = 0;
  };

  /** @brief Renders the HTTP/server startup presentation to stderr. */
  class StartupPresentation final
  {
  public:
    static bool stdout_is_tty()
    {
#if defined(_WIN32)
      return true;
#else
      return ::isatty(::fileno(stdout)) == 1;
#endif
    }

    static bool stderr_is_tty()
    {
#if defined(_WIN32)
      return true;
#else
      return ::isatty(::fileno(stderr)) == 1;
#endif
    }

    static bool colors_enabled()
    {
      if (!vix::env::get_or("NO_COLOR").empty())
        return false;

      const std::string value = lower(vix::env::get_or("VIX_COLOR"));
      if (value == "never" || value == "0" || value == "false")
        return false;
      if (value == "always" || value == "1" || value == "true")
        return true;
      return true;
    }

    static std::string mode_from_env()
    {
      const std::string value = lower(vix::env::get_or("VIX_MODE"));
      if (value == "dev" || value == "watch" || value == "reload")
        return "dev";
      return "run";
    }

    static bool hyperlinks_enabled()
    {
      if (!vix::env::get_or("VIX_NO_HYPERLINK").empty() || !stderr_is_tty())
        return false;

      if (vix::env::has("VSCODE_PID") || vix::env::has("WT_SESSION") ||
          vix::env::has("WEZTERM_EXECUTABLE") || vix::env::has("KITTY_WINDOW_ID") ||
          vix::env::has("VTE_VERSION"))
        return true;

      const std::string program = vix::env::get_or("TERM_PROGRAM");
      if (program == "iTerm.app" || program == "Apple_Terminal" ||
          program == "WezTerm" || program == "vscode")
        return true;

      return false;
    }

    static std::string osc8_link(const std::string &url, const std::string &text, bool on)
    {
      if (!on)
        return text;
      return "\033]8;;" + url + "\033\\" + text + "\033]8;;\033\\";
    }

    static void emit_server_ready(const ServerReadyInfo &info)
    {
      vix::log::console_reset_banner();
      const bool color = colors_enabled();
      const std::string http_url = make_http_url(info);
      const std::string ws_url = info.show_ws ? make_ws_url(info) : std::string{};

      {
        std::lock_guard<std::mutex> lock(vix::log::console_mutex());
        const std::string timestamp = format_local_time_12h();
        if (color)
          std::cerr << "\033[0m";

        std::cerr << (color ? gray(timestamp) : timestamp) << "  "
                  << identity(info.app, color) << "  "
                  << status_pill(upper(info.status), color);
        if (!info.version.empty())
          std::cerr << "  " << (color ? bold(white(info.version)) : info.version);
        if (info.ready_ms >= 0)
        {
          const std::string milliseconds = " (" + std::to_string(info.ready_ms) + " ms)";
          std::cerr << (color ? subtle(milliseconds) : milliseconds);
        }
        if (!info.mode.empty())
          std::cerr << "  " << mode_tag(info.mode, color);
        std::cerr << "\n";

        row(bullet(color), info.scheme == "https" ? "HTTPS:" : "HTTP:", http_url, false, color);
        if (info.show_ws)
          row(bullet(color), "WS:", ws_url, false, color);
        if (!info.config_path.empty())
          row(info_mark(color), "Config:", info.config_path, true, color);
        if (info.threads > 0)
        {
          std::string count = std::to_string(info.threads);
          if (info.max_threads > 0)
            count += "/" + std::to_string(info.max_threads);
          row(info_mark(color), "Threads:", count, true, color);
        }
        row(info_mark(color), "Mode:", pretty_mode(info.mode), true, color);
        row(info_mark(color), "Status:", pretty_status(info.status), true, color);
        if (info.show_hints)
          row(info_mark(color), "Hint:", "Ctrl+C to stop the server", true, color);
        std::cerr << "\r";
        std::cerr.flush();
      }

      vix::log::console_mark_banner_done();
    }

  private:
    static std::string lower(std::string value)
    {
      for (char &character : value)
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
      return value;
    }

    static std::string upper(std::string value)
    {
      for (char &character : value)
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
      return value;
    }

    static bool animations_enabled()
    {
      return vix::env::get_or("VIX_NO_ANIM").empty() && stderr_is_tty() &&
             vix::env::get_or("NO_COLOR").empty();
    }

    static std::string wrap(const char *code, const std::string &value)
    {
      return std::string(code) + value + "\033[0m";
    }

    static std::string gray(const std::string &value) { return wrap("\033[90m", value); }
    static std::string white(const std::string &value) { return wrap("\033[97m", value); }
    static std::string cyan(const std::string &value) { return wrap("\033[36m", value); }
    static std::string bold(const std::string &value) { return wrap("\033[1m", value); }
    static std::string subtle(const std::string &value) { return wrap("\033[38;5;110m", value); }

    static std::string mode_tag(const std::string &mode, bool color)
    {
      if (!color)
        return mode == "dev" ? "[dev]" : "[run]";
      if (mode == "dev")
      {
        if (!animations_enabled())
          return "[dev]";
        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        const int phase = static_cast<int>((milliseconds / 300) % 3);
        const int background = phase == 0 ? 28 : (phase == 1 ? 34 : 40);
        return "\033[1m\033[48;5;" + std::to_string(background) +
               "m\033[30m dev\033[0m";
      }
      return "\033[1m\033[48;5;238m\033[97m run\033[0m";
    }

    static std::string identity(const std::string &app, bool color)
    {
      if (!color)
        return "[" + app + "]";
      std::string name = app;
      if (name == "vix.cpp" || name == "VIX.cpp" || name == "Vix.cpp")
        name = "Vix.cpp";
      return bold(wrap("\033[32m", name));
    }

    static std::string status_pill(const std::string &status, bool color)
    {
      if (!color)
        return status;
      int background = 34;
      if (status == "RUNNING" || status == "LISTENING") background = 35;
      else if (status == "WARN" || status == "WARNING") background = 214;
      else if (status == "ERROR" || status == "FAILED") background = 196;
      return "\033[1m\033[48;5;" + std::to_string(background) +
             "m\033[30m " + status + " \033[0m";
    }

    static std::string format_local_time_12h()
    {
      const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
      std::tm time{};
#if defined(_WIN32)
      localtime_s(&time, &now);
#else
      localtime_r(&now, &time);
#endif
      int hour = time.tm_hour % 12;
      if (hour == 0) hour = 12;
      std::ostringstream output;
      output << hour << ':' << std::setw(2) << std::setfill('0') << time.tm_min << ':'
             << std::setw(2) << std::setfill('0') << time.tm_sec
             << (time.tm_hour >= 12 ? " PM" : " AM");
      return output.str();
    }

    static std::string make_http_url(const ServerReadyInfo &info)
    {
      std::string path = info.base_path;
      if (path.empty()) path = "/";
      else if (path.front() != '/') path.insert(path.begin(), '/');
      return info.scheme + "://" + info.host + ':' + std::to_string(info.port) + path;
    }

    static std::string make_ws_url(const ServerReadyInfo &info)
    {
      std::string path = info.ws_path;
      if (!path.empty() && path.front() != '/') path.insert(path.begin(), '/');
      return info.ws_scheme + "://" + info.ws_host + ':' + std::to_string(info.ws_port) + path;
    }

    static std::string pretty_mode(const std::string &mode)
    {
      if (mode == "dev") return "dev (watch/reload)";
      return mode.empty() ? "run" : mode;
    }

    static std::string pretty_status(const std::string &status)
    {
      return status.empty() ? "ready" : status;
    }

    static std::string bullet(bool color) { return color ? cyan(">") : ">"; }
    static std::string info_mark(bool color) { return color ? gray("-") : "-"; }

    static std::string link(const std::string &url, bool color)
    {
      return osc8_link(url, color ? cyan(url) : url, hyperlinks_enabled());
    }

    static void row(const std::string &icon, const std::string &label,
                    const std::string &value, bool dim_value, bool color)
    {
      std::string padded = label;
      if (padded.size() < 8) padded.append(8 - padded.size(), ' ');
      const std::string rendered_value = dim_value
          ? (color ? wrap("\033[2m", value) : value)
          : link(value, color);
      std::cerr << "  " << (color ? "\033[0m" + icon : icon) << " "
                << (color ? bold(white(padded)) : padded) << " "
                << rendered_value << "\n";
    }
  };
} // namespace vix::server

#endif // VIX_SERVER_READY_PRESENTATION_HPP
