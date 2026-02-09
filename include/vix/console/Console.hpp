/**
 *
 *  @file Console.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 *  Vix Console (core) — Dev-proof, Zero-config
 */
#ifndef VIX_CONSOLE_CLASS_HPP
#define VIX_CONSOLE_CLASS_HPP

#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <vix/utils/Env.hpp>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#include <cstdio>
#include <time.h>
#endif

namespace vix
{
  class Console final
  {
  public:
    enum class Level : uint8_t
    {
      Debug = 0,
      Info = 1,
      Warn = 2,
      Error = 3,
      Off = 4
    };

    // (zero-config)
    template <typename... Ts>
    void log(Ts &&...xs) noexcept { write_(Level::Info, Stream::Out, /*rate_limit*/ true, std::forward<Ts>(xs)...); }

    template <typename... Ts>
    void info(Ts &&...xs) noexcept { write_(Level::Info, Stream::Out, /*rate_limit*/ true, std::forward<Ts>(xs)...); }

    template <typename... Ts>
    void warn(Ts &&...xs) noexcept { write_(Level::Warn, Stream::Err, /*rate_limit*/ false, std::forward<Ts>(xs)...); }

    template <typename... Ts>
    void error(Ts &&...xs) noexcept { write_(Level::Error, Stream::Err, /*rate_limit*/ false, std::forward<Ts>(xs)...); }

    template <typename... Ts>
    void debug(Ts &&...xs) noexcept { write_(Level::Debug, Stream::Out, /*rate_limit*/ false, std::forward<Ts>(xs)...); }

    void set_level(Level lvl) noexcept { level_.store(lvl, std::memory_order_relaxed); }
    Level level() const noexcept { return level_.load(std::memory_order_relaxed); }

  public:
    // Global constructor reads env once (stable, zero-config for dev).
    Console() noexcept
    {
      init_from_env_once_();
    }

    Console(const Console &) = delete;
    Console &operator=(const Console &) = delete;
    Console(Console &&) = delete;
    Console &operator=(Console &&) = delete;

  private:
    enum class Stream : uint8_t
    {
      Out,
      Err
    };

    enum class ColorMode : uint8_t
    {
      Auto,
      Always,
      Never
    };

    //  Fixed buffer builder (no iostream, no dynamic allocations)
    struct LineBuffer final
    {
      static constexpr std::size_t kCap = 8192;

      char buf[kCap];
      std::size_t len = 0;
      bool truncated = false;

      void reset() noexcept
      {
        len = 0;
        truncated = false;
      }

      void push_char(char c) noexcept
      {
        if (len + 1 >= kCap)
        {
          truncated = true;
          return;
        }
        buf[len++] = c;
      }

      void push_str(std::string_view s) noexcept
      {
        if (s.empty())
          return;
        const std::size_t space = (kCap > len ? (kCap - len) : 0);
        if (space <= 1)
        {
          truncated = true;
          return;
        }

        const std::size_t n = (s.size() < (space - 1)) ? s.size() : (space - 1);
        if (n == 0)
        {
          truncated = true;
          return;
        }

        std::memcpy(buf + len, s.data(), n);
        len += n;

        if (n < s.size())
          truncated = true;
      }

      void push_space() noexcept
      {
        if (len == 0)
          return;
        push_char(' ');
      }

      void push_newline() noexcept
      {
        push_char('\n');
      }

      void finalize_truncation_marker() noexcept
      {
        if (!truncated)
          return;
        // Append minimal marker if space remains: " ..."
        const std::string_view mark = " ...";
        push_str(mark);
      }
    };

    // Internal state (env read once, stable)
    std::atomic<Level> level_{Level::Info};
    std::atomic<ColorMode> color_mode_{ColorMode::Auto};

    // Cached TTY detection (read once).
    bool stdout_tty_ = false;
    bool stderr_tty_ = false;

    // Rate limiting for log/info only (global process, 1s window).
    // - protects against hot-loop spam without becoming a logger.
    static constexpr uint32_t kRateLimitPerSec = 200;

    std::atomic<std::uint64_t> rl_epoch_sec_{0};       // current window second
    std::atomic<std::uint32_t> rl_count_{0};           // log/info count in window
    std::atomic<std::uint32_t> rl_suppressed_{0};      // suppressed count in window
    std::atomic<std::uint64_t> rl_last_report_sec_{0}; // last second we emitted suppression line

    // Global mutex: ensure atomic line writes (no interleaving).
    static std::mutex &mutex_() noexcept
    {
      static std::mutex m;
      return m;
    }

  private:
    // Platform helpers
    static bool is_tty_stdout_() noexcept
    {
#if defined(_WIN32)
      // Conservative: many Windows terminals support ANSI now; treat as TTY.
      return true;
#else
      return ::isatty(STDOUT_FILENO) == 1;
#endif
    }

    static bool is_tty_stderr_() noexcept
    {
#if defined(_WIN32)
      return true;
#else
      return ::isatty(STDERR_FILENO) == 1;
#endif
    }

    static std::string lower_copy_(std::string_view in) noexcept
    {
      std::string out;
      out.reserve(in.size());
      for (char ch : in)
      {
        const unsigned char uc = static_cast<unsigned char>(ch);
        out.push_back(static_cast<char>(std::tolower(uc)));
      }
      return out;
    }

    static Level parse_level_(std::string_view s) noexcept
    {
      const std::string v = lower_copy_(s);

      if (v == "off" || v == "none" || v == "silent" || v == "never" || v == "0")
        return Level::Off;
      if (v == "error" || v == "err")
        return Level::Error;
      if (v == "warn" || v == "warning")
        return Level::Warn;
      if (v == "info" || v == "log")
        return Level::Info;
      if (v == "debug" || v == "trace")
        return Level::Debug;

      return Level::Info;
    }

    static ColorMode parse_color_mode_(std::string_view s) noexcept
    {
      const std::string v = lower_copy_(s);
      if (v == "always" || v == "1" || v == "true")
        return ColorMode::Always;
      if (v == "never" || v == "0" || v == "false")
        return ColorMode::Never;
      return ColorMode::Auto;
    }

    void init_from_env_once_() noexcept
    {
      // 1) TTY detection once
      stdout_tty_ = is_tty_stdout_();
      stderr_tty_ = is_tty_stderr_();

      // 2) Level from env (stable)
      if (const char *raw = vix::utils::vix_getenv("VIX_CONSOLE_LEVEL"); raw && *raw)
        level_.store(parse_level_(raw), std::memory_order_relaxed);

      // 3) Colors from env (stable)
      // NO_COLOR forces Never
      if (const char *nc = vix::utils::vix_getenv("NO_COLOR"); nc && *nc)
      {
        color_mode_.store(ColorMode::Never, std::memory_order_relaxed);
      }
      else if (const char *cm = vix::utils::vix_getenv("VIX_COLOR"); cm && *cm)
      {
        color_mode_.store(parse_color_mode_(cm), std::memory_order_relaxed);
      }
      else
      {
        color_mode_.store(ColorMode::Auto, std::memory_order_relaxed);
      }

      // Initialize rate limiter epoch to current sec (optional, safe).
      rl_epoch_sec_.store(now_epoch_sec_(), std::memory_order_relaxed);
      rl_count_.store(0, std::memory_order_relaxed);
      rl_suppressed_.store(0, std::memory_order_relaxed);
      rl_last_report_sec_.store(0, std::memory_order_relaxed);
    }

    bool colors_enabled_(Stream s) const noexcept
    {
      // NO_COLOR already handled in init; still respect it if set late.
      if (const char *nc = vix::utils::vix_getenv("NO_COLOR"); nc && *nc)
        return false;

      const ColorMode m = color_mode_.load(std::memory_order_relaxed);
      if (m == ColorMode::Never)
        return false;
      if (m == ColorMode::Always)
        return true;

      // Auto: only if TTY
      return (s == Stream::Out) ? stdout_tty_ : stderr_tty_;
    }

    static const char *level_tag_(Level lvl) noexcept
    {
      switch (lvl)
      {
      case Level::Debug:
        return "debug";
      case Level::Info:
        return "info";
      case Level::Warn:
        return "warn";
      case Level::Error:
        return "error";
      case Level::Off:
        return "off";
      }
      return "info";
    }

    // ANSI coloring: sober/stable
    static std::string_view level_color_code_(Level lvl) noexcept
    {
      switch (lvl)
      {
      case Level::Debug:
        return "\033[36m"; // cyan
      case Level::Info:
        return "\033[32m"; // green
      case Level::Warn:
        return "\033[33m"; // yellow
      case Level::Error:
        return "\033[31m"; // red
      case Level::Off:
        return "\033[90m"; // gray
      }
      return "\033[32m";
    }

    static std::uint64_t now_epoch_sec_() noexcept
    {
      using namespace std::chrono;
      const auto now = system_clock::now();
      const auto sec = duration_cast<seconds>(now.time_since_epoch()).count();
      return static_cast<std::uint64_t>(sec);
    }

    static void format_hms_(char out[9]) noexcept
    {
      // HH:MM:SS (local time)
#if defined(_WIN32)
      SYSTEMTIME st;
      GetLocalTime(&st);
      const int hh = static_cast<int>(st.wHour);
      const int mm = static_cast<int>(st.wMinute);
      const int ss = static_cast<int>(st.wSecond);
#else
      using clock = std::chrono::system_clock;
      const std::time_t t = clock::to_time_t(clock::now());
      std::tm tm{};
      localtime_r(&t, &tm);
      const int hh = tm.tm_hour;
      const int mm = tm.tm_min;
      const int ss = tm.tm_sec;
#endif
      out[0] = static_cast<char>('0' + (hh / 10));
      out[1] = static_cast<char>('0' + (hh % 10));
      out[2] = ':';
      out[3] = static_cast<char>('0' + (mm / 10));
      out[4] = static_cast<char>('0' + (mm % 10));
      out[5] = ':';
      out[6] = static_cast<char>('0' + (ss / 10));
      out[7] = static_cast<char>('0' + (ss % 10));
      out[8] = '\0';
    }

    // ---- Low-level write (Mojo-like) ----
    static void write_fd_(Stream s, const char *data, std::size_t n) noexcept
    {
      if (!data || n == 0)
        return;

#if defined(_WIN32)
      HANDLE h = (s == Stream::Err) ? GetStdHandle(STD_ERROR_HANDLE) : GetStdHandle(STD_OUTPUT_HANDLE);
      if (h == INVALID_HANDLE_VALUE || h == nullptr)
        return;

      DWORD written = 0;
      // Best-effort: ignore failures.
      (void)WriteFile(h, data, static_cast<DWORD>(n), &written, nullptr);
#else
      const int fd = (s == Stream::Err) ? STDERR_FILENO : STDOUT_FILENO;

      const char *p = data;
      std::size_t left = n;

      while (left > 0)
      {
        const ssize_t r = ::write(fd, p, left);
        if (r <= 0)
          return; // best-effort
        p += static_cast<std::size_t>(r);
        left -= static_cast<std::size_t>(r);
      }
#endif
    }

    static void flush_stream_(Stream s) noexcept
    {
#if defined(_WIN32)
      // No explicit flush for std handles here (best-effort).
      (void)s;
#else
      // Best-effort: use fsync? Not appropriate; skip.
      (void)s;
#endif
    }

    // Fast level gating (critical)
    bool enabled_(Level msg_lvl) const noexcept
    {
      const Level cur = level_.load(std::memory_order_relaxed);
      if (cur == Level::Off)
        return false;
      return static_cast<uint8_t>(msg_lvl) >= static_cast<uint8_t>(cur);
    }

    // Rate limiting (log/info only)
    bool rate_allow_or_suppress_(std::uint64_t now_sec) noexcept
    {
      std::uint64_t epoch = rl_epoch_sec_.load(std::memory_order_relaxed);
      if (epoch != now_sec)
      {
        // Try to advance window. If raced, that's fine.
        if (rl_epoch_sec_.compare_exchange_strong(epoch, now_sec, std::memory_order_relaxed))
        {
          rl_count_.store(0, std::memory_order_relaxed);
          rl_suppressed_.store(0, std::memory_order_relaxed);
        }
        else
        {
          // Another thread moved it; proceed.
        }
      }

      const uint32_t c = rl_count_.fetch_add(1, std::memory_order_relaxed) + 1;
      if (c <= kRateLimitPerSec)
        return true;

      rl_suppressed_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }

    void maybe_emit_suppressed_line_(std::uint64_t now_sec) noexcept
    {
      // Emit at most once per second, and only if suppressed > 0.
      const uint32_t suppressed = rl_suppressed_.load(std::memory_order_relaxed);
      if (suppressed == 0)
        return;

      std::uint64_t last = rl_last_report_sec_.load(std::memory_order_relaxed);
      if (last == now_sec)
        return;

      if (!rl_last_report_sec_.compare_exchange_strong(last, now_sec, std::memory_order_relaxed))
        return;

      // Build and emit a WARN line to stderr; do not recurse into rate limiting.
      LineBuffer lb;
      build_prefix_(lb, Level::Warn, Stream::Err);
      lb.push_str("(console) suppressed ");
      append_u32_(lb, suppressed);
      lb.push_str(" log/info lines (rate limit)");
      lb.push_newline();
      lb.finalize_truncation_marker(); // safe no-op if not truncated

      std::lock_guard<std::mutex> lk(mutex_());
      write_fd_(Stream::Err, lb.buf, lb.len);
    }

    // Prefix builder: "HH:MM:SS [level] "
    void build_prefix_(LineBuffer &lb, Level lvl, Stream stream) const noexcept
    {
      const bool color = colors_enabled_(stream);

      char hms[9];
      format_hms_(hms);

      if (color)
      {
        // dim timestamp
        lb.push_str("\033[90m");
        lb.push_str(hms);
        lb.push_str("\033[0m");
        lb.push_char(' ');
      }
      else
      {
        lb.push_str(hms);
        lb.push_char(' ');
      }

      // [level]
      lb.push_char('[');
      if (color)
      {
        lb.push_str(level_color_code_(lvl));
        lb.push_str(level_tag_(lvl));
        lb.push_str("\033[0m");
      }
      else
      {
        lb.push_str(level_tag_(lvl));
      }
      lb.push_char(']');
      lb.push_char(' ');
    }

    // Argument formatting (no ostringstream)
    static void append_u32_(LineBuffer &lb, std::uint32_t v) noexcept
    {
      char tmp[16];
      // manual itoa-like, small and safe
      int i = 0;
      if (v == 0)
      {
        lb.push_char('0');
        return;
      }
      while (v > 0 && i < 15)
      {
        tmp[i++] = static_cast<char>('0' + (v % 10));
        v /= 10;
      }
      for (int j = i - 1; j >= 0; --j)
        lb.push_char(tmp[j]);
    }

    template <typename Int>
    static void append_int_(LineBuffer &lb, Int v) noexcept
    {
      static_assert(std::is_integral_v<Int>, "integral required");
      char tmp[32];
      char *p = tmp;
      char *end = tmp + sizeof(tmp);

      // Handle sign
      using U = std::make_unsigned_t<Int>;
      U u = 0;
      bool neg = false;

      if constexpr (std::is_signed_v<Int>)
      {
        if (v < 0)
        {
          neg = true;
          u = static_cast<U>(-(v + 1)) + 1;
        }
        else
        {
          u = static_cast<U>(v);
        }
      }
      else
      {
        u = static_cast<U>(v);
      }

      char digits[32];
      int i = 0;
      if (u == 0)
      {
        digits[i++] = '0';
      }
      while (u > 0 && i < 31)
      {
        digits[i++] = static_cast<char>('0' + (u % 10));
        u /= 10;
      }

      if (neg)
        lb.push_char('-');
      for (int j = i - 1; j >= 0; --j)
        lb.push_char(digits[j]);

      (void)p;
      (void)end;
    }

    static void append_float_(LineBuffer &lb, double v) noexcept
    {
      // Use snprintf into stack buffer (still no allocations).
      char tmp[64];
#if defined(_WIN32)
      int n = _snprintf_s(tmp, sizeof(tmp), _TRUNCATE, "%.6g", v);
#else
      int n = std::snprintf(tmp, sizeof(tmp), "%.6g", v);
#endif
      if (n <= 0)
      {
        lb.push_str("0");
        return;
      }
      lb.push_str(std::string_view(tmp, static_cast<std::size_t>(n)));
    }

    static void append_ptr_(LineBuffer &lb, const void *p) noexcept
    {
      if (!p)
      {
        lb.push_str("null");
        return;
      }
      // hex address
      std::uintptr_t v = reinterpret_cast<std::uintptr_t>(p);
      lb.push_str("0x");
      char digits[2 * sizeof(std::uintptr_t)];
      int i = 0;
      while (v > 0 && i < (int)sizeof(digits))
      {
        const int nib = int(v & 0xF);
        digits[i++] = static_cast<char>(nib < 10 ? ('0' + nib) : ('a' + (nib - 10)));
        v >>= 4;
      }
      if (i == 0)
        digits[i++] = '0';
      for (int j = i - 1; j >= 0; --j)
        lb.push_char(digits[j]);
    }

    template <typename T>
    static constexpr bool is_string_like_v =
        std::is_same_v<std::decay_t<T>, std::string> ||
        std::is_same_v<std::decay_t<T>, std::string_view> ||
        std::is_same_v<std::decay_t<T>, const char *> ||
        std::is_same_v<std::decay_t<T>, char *>;

    template <typename T>
    static void append_one_(LineBuffer &lb, T &&x) noexcept
    {
      using D = std::decay_t<T>;

      if constexpr (std::is_same_v<D, std::string>)
      {
        lb.push_str(std::string_view(x));
      }
      else if constexpr (std::is_same_v<D, std::string_view>)
      {
        lb.push_str(x);
      }
      else if constexpr (std::is_same_v<D, const char *> || std::is_same_v<D, char *>)
      {
        if (x)
          lb.push_str(std::string_view(x));
        else
          lb.push_str("null");
      }
      else if constexpr (std::is_same_v<D, bool>)
      {
        lb.push_str(x ? "true" : "false");
      }
      else if constexpr (std::is_integral_v<D> && !std::is_same_v<D, bool>)
      {
        append_int_(lb, x);
      }
      else if constexpr (std::is_floating_point_v<D>)
      {
        append_float_(lb, static_cast<double>(x));
      }
      else if constexpr (std::is_enum_v<D>)
      {
        using U = std::underlying_type_t<D>;
        append_int_(lb, static_cast<U>(x));
      }
      else if constexpr (std::is_pointer_v<D>)
      {
        append_ptr_(lb, reinterpret_cast<const void *>(x));
      }
      else if constexpr (std::is_convertible_v<D, std::string_view>)
      {
        lb.push_str(static_cast<std::string_view>(x));
      }
      else
      {
        lb.push_str("[object]");
      }
    }

    template <typename... Ts>
    static void append_args_(LineBuffer &lb, Ts &&...xs) noexcept
    {
      bool first = true;
      auto add = [&](auto &&v) noexcept
      {
        if (!first)
          lb.push_char(' ');
        first = false;
        append_one_(lb, std::forward<decltype(v)>(v));
      };
      (add(std::forward<Ts>(xs)), ...);
    }

    // Main write path (fast gating, build outside lock, single write under lock)
    template <typename... Ts>
    void write_(Level lvl, Stream stream, bool rate_limit, Ts &&...xs) noexcept
    {
      // (1) Fast gating: filtered == almost free.
      if (!enabled_(lvl))
        return;

      // (2) Rate limit only for log/info (dev-proof). Warn/error never suppressed.
      const std::uint64_t now_sec = now_epoch_sec_();
      if (rate_limit)
      {
        if (!rate_allow_or_suppress_(now_sec))
        {
          // Best-effort: still occasionally report suppression.
          maybe_emit_suppressed_line_(now_sec);
          return;
        }
        // Also report any pending suppression from previous second(s).
        maybe_emit_suppressed_line_(now_sec);
      }

      // (3) Build complete line outside lock
      LineBuffer lb;
      build_prefix_(lb, lvl, stream);
      append_args_(lb, std::forward<Ts>(xs)...);

      lb.push_newline();
      lb.finalize_truncation_marker();

      // (4) Atomic line write (one syscall-ish) under lock (no interleaving).
      {
        std::lock_guard<std::mutex> lk(mutex_());
        write_fd_(stream, lb.buf, lb.len);
      }

      // (5) I/O policy: do not flush by default; optionally flush on error.
      if (lvl == Level::Error)
      {
      }
    }
  };

  // Global singleton vix::console.info(...)
  inline Console console;

} // namespace vix

#endif // VIX_CONSOLE_CLASS_HPP
