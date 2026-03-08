/**
 *
 *  @file print.hpp
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
#ifndef VIX_PRINT_CLASS_HPP
#define VIX_PRINT_CLASS_HPP

#include <algorithm>
#include <any>
#include <array>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <forward_list>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <ostream>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#include <expected>
#define VIX_HAS_EXPECTED 1
#else
#define VIX_HAS_EXPECTED 0
#endif

#include <cwchar>
#include <locale>
#include <codecvt>

namespace vix
{
  /// @brief Primary formatter template — specialize for custom types.
  template <typename T, typename = void>
  struct formatter;

  namespace detail
  {

    template <typename T>
    void write(std::ostream &os, const T &value);

  } // namespace detail

  namespace traits
  {

    // Has operator<<
    template <typename T, typename = void>
    struct is_ostreamable : std::false_type
    {
    };

    template <typename T>
    struct is_ostreamable<T,
                          std::void_t<decltype(std::declval<std::ostream &>() << std::declval<const T &>())>>
        : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_ostreamable_v = is_ostreamable<T>::value;

    // Is string-like (treat as atom, not range)
    template <typename T>
    struct is_string_like : std::false_type
    {
    };

    template <>
    struct is_string_like<std::string> : std::true_type
    {
    };
    template <>
    struct is_string_like<std::string_view> : std::true_type
    {
    };
    template <>
    struct is_string_like<const char *> : std::true_type
    {
    };
    template <>
    struct is_string_like<char *> : std::true_type
    {
    };
    template <>
    struct is_string_like<std::wstring> : std::true_type
    {
    };
    template <>
    struct is_string_like<std::wstring_view> : std::true_type
    {
    };
    template <>
    struct is_string_like<const wchar_t *> : std::true_type
    {
    };

    template <std::size_t N>
    struct is_string_like<char[N]> : std::true_type
    {
    };

    template <std::size_t N>
    struct is_string_like<const char[N]> : std::true_type
    {
    };

    template <std::size_t N>
    struct is_string_like<wchar_t[N]> : std::true_type
    {
    };

    template <std::size_t N>
    struct is_string_like<const wchar_t[N]> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_string_like_v = is_string_like<std::remove_cvref_t<T>>::value;

    // Is range (iterable, not string)
    template <typename T, typename = void>
    struct is_range : std::false_type
    {
    };

    template <typename T>
    struct is_range<T,
                    std::void_t<decltype(std::begin(std::declval<const T &>())),
                                decltype(std::end(std::declval<const T &>()))>>
        : std::bool_constant<!is_string_like_v<T>>
    {
    };

    template <typename T>
    inline constexpr bool is_range_v = is_range<T>::value;

    // Is map-like (has key_type and mapped_type)
    template <typename T, typename = void>
    struct is_map_like : std::false_type
    {
    };

    template <typename T>
    struct is_map_like<T,
                       std::void_t<typename T::key_type, typename T::mapped_type>>
        : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_map_like_v = is_map_like<T>::value;

    // Is set-like (has key_type, no mapped_type)
    template <typename T, typename = void>
    struct is_set_like : std::false_type
    {
    };

    template <typename T>
    struct is_set_like<T,
                       std::void_t<typename T::key_type>>
        : std::bool_constant<!is_map_like_v<T>>
    {
    };

    template <typename T>
    inline constexpr bool is_set_like_v = is_set_like<T>::value;

    // Is tuple-like
    template <typename T, typename = void>
    struct is_tuple_like : std::false_type
    {
    };

    template <typename T>
    struct is_tuple_like<T,
                         std::void_t<decltype(std::tuple_size<T>::value)>>
        : std::true_type
    {
    };

    // std::string has no tuple_size normally, but guard anyway
    template <>
    struct is_tuple_like<std::string> : std::false_type
    {
    };

    template <typename T>
    inline constexpr bool is_tuple_like_v = is_tuple_like<std::remove_cvref_t<T>>::value;

    // Is pair
    template <typename T>
    struct is_pair : std::false_type
    {
    };

    template <typename A, typename B>
    struct is_pair<std::pair<A, B>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_pair_v = is_pair<std::remove_cvref_t<T>>::value;

    // Is smart pointer
    template <typename T>
    struct is_unique_ptr : std::false_type
    {
    };
    template <typename T, typename D>
    struct is_unique_ptr<std::unique_ptr<T, D>> : std::true_type
    {
    };

    template <typename T>
    struct is_shared_ptr : std::false_type
    {
    };
    template <typename T>
    struct is_shared_ptr<std::shared_ptr<T>> : std::true_type
    {
    };

    template <typename T>
    struct is_weak_ptr : std::false_type
    {
    };
    template <typename T>
    struct is_weak_ptr<std::weak_ptr<T>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_smart_ptr_v =
        is_unique_ptr<std::remove_cvref_t<T>>::value ||
        is_shared_ptr<std::remove_cvref_t<T>>::value ||
        is_weak_ptr<std::remove_cvref_t<T>>::value;

    // Is optional
    template <typename T>
    struct is_optional : std::false_type
    {
    };
    template <typename T>
    struct is_optional<std::optional<T>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_optional_v = is_optional<std::remove_cvref_t<T>>::value;

    // Is variant
    template <typename T>
    struct is_variant : std::false_type
    {
    };
    template <typename... Ts>
    struct is_variant<std::variant<Ts...>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_variant_v = is_variant<std::remove_cvref_t<T>>::value;

    // Is std::any
    template <typename T>
    inline constexpr bool is_any_v = std::is_same_v<std::remove_cvref_t<T>, std::any>;

    // Is chrono duration
    template <typename T>
    struct is_duration : std::false_type
    {
    };
    template <typename Rep, typename Period>
    struct is_duration<std::chrono::duration<Rep, Period>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_duration_v = is_duration<std::remove_cvref_t<T>>::value;

    // Is chrono time_point
    template <typename T>
    struct is_time_point : std::false_type
    {
    };
    template <typename Clock, typename Duration>
    struct is_time_point<std::chrono::time_point<Clock, Duration>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_time_point_v = is_time_point<std::remove_cvref_t<T>>::value;

    // Is filesystem::path
    template <typename T>
    inline constexpr bool is_fs_path_v =
        std::is_same_v<std::remove_cvref_t<T>, std::filesystem::path>;

    // Is reference_wrapper
    template <typename T>
    struct is_reference_wrapper : std::false_type
    {
    };
    template <typename T>
    struct is_reference_wrapper<std::reference_wrapper<T>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_reference_wrapper_v =
        is_reference_wrapper<std::remove_cvref_t<T>>::value;

    // Is raw pointer (non-function, non-char)
    template <typename T>
    inline constexpr bool is_raw_pointer_v =
        std::is_pointer_v<T> &&
        !is_string_like_v<T> &&
        !std::is_function_v<std::remove_pointer_t<T>>;

    // Is function pointer
    template <typename T>
    inline constexpr bool is_function_ptr_v =
        std::is_pointer_v<T> &&
        std::is_function_v<std::remove_pointer_t<T>>;

    // ADL vix_format detection
    template <typename T, typename = void>
    struct has_vix_format : std::false_type
    {
    };

    template <typename T>
    struct has_vix_format<T,
                          std::void_t<decltype(vix_format(std::declval<std::ostream &>(),
                                                          std::declval<const T &>()))>>
        : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool has_vix_format_v = has_vix_format<T>::value;

    // Has vix::formatter specialization
    // Forward declared — evaluated later after formatter is defined.

    // Is stack/queue adapter
    template <typename T>
    struct is_stack : std::false_type
    {
    };
    template <typename T, typename C>
    struct is_stack<std::stack<T, C>> : std::true_type
    {
    };

    template <typename T>
    struct is_queue : std::false_type
    {
    };
    template <typename T, typename C>
    struct is_queue<std::queue<T, C>> : std::true_type
    {
    };

    template <typename T>
    struct is_priority_queue : std::false_type
    {
    };
    template <typename T, typename C, typename Cmp>
    struct is_priority_queue<std::priority_queue<T, C, Cmp>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_adapter_v =
        is_stack<std::remove_cvref_t<T>>::value ||
        is_queue<std::remove_cvref_t<T>>::value ||
        is_priority_queue<std::remove_cvref_t<T>>::value;

    // Is enum
    template <typename T>
    inline constexpr bool is_enum_v = std::is_enum_v<std::remove_cvref_t<T>>;

    // 2.22  Is bool (must be checked before integral)
    template <typename T>
    inline constexpr bool is_bool_v = std::is_same_v<std::remove_cvref_t<T>, bool>;

    // Is char types
    template <typename T>
    inline constexpr bool is_char_v =
        std::is_same_v<std::remove_cvref_t<T>, char> ||
        std::is_same_v<std::remove_cvref_t<T>, signed char> ||
        std::is_same_v<std::remove_cvref_t<T>, unsigned char> ||
        std::is_same_v<std::remove_cvref_t<T>, wchar_t> ||
        std::is_same_v<std::remove_cvref_t<T>, char8_t> ||
        std::is_same_v<std::remove_cvref_t<T>, char16_t> ||
        std::is_same_v<std::remove_cvref_t<T>, char32_t>;

    // Is nullptr_t
    template <typename T>
    inline constexpr bool is_nullptr_v =
        std::is_same_v<std::remove_cvref_t<T>, std::nullptr_t>;

#if VIX_HAS_EXPECTED
    // Is std::expected
    template <typename T>
    struct is_expected : std::false_type
    {
    };
    template <typename T, typename E>
    struct is_expected<std::expected<T, E>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_expected_v = is_expected<std::remove_cvref_t<T>>::value;
#endif

  } // namespace traits

  namespace concepts
  {

    /// Streamable via operator<<
    template <typename T>
    concept Streamable = traits::is_ostreamable_v<T>;

    /// Iterable range that is not a string
    template <typename T>
    concept Range = traits::is_range_v<T>;

    /// Map-like associative container
    template <typename T>
    concept MapLike = traits::is_map_like_v<T>;

    /// Set-like associative container
    template <typename T>
    concept SetLike = traits::is_set_like_v<T> && !MapLike<T>;

    /// Tuple-like (pair, tuple, array)
    template <typename T>
    concept TupleLike = traits::is_tuple_like_v<T> && !Range<T>;

    /// Has ADL vix_format hook
    template <typename T>
    concept HasVixFormat = traits::has_vix_format_v<T>;

    /// Smart pointer
    template <typename T>
    concept SmartPointer = traits::is_smart_ptr_v<T>;

    /// Chrono duration
    template <typename T>
    concept Duration = traits::is_duration_v<T>;

    /// Chrono time_point
    template <typename T>
    concept TimePoint = traits::is_time_point_v<T>;

  } // namespace concepts

  /// @brief Controls rendering behaviour of vix::print.
  struct print_config
  {
    std::string separator = " ";    ///< Between multiple arguments
    std::string end = "\n";         ///< After all arguments
    std::ostream *out = &std::cout; ///< Output stream
    bool color = false;             ///< ANSI color (experimental)
    std::size_t max_items = 256;    ///< Max items shown in a container
    bool show_type = false;         ///< Show type annotations
    bool compact = false;           ///< Compact single-line output
    std::string indent_str = "  ";  ///< Indentation unit (unused in compact)
  };

  /// @brief Thread-local default config (modifiable by user).
  inline print_config &default_config() noexcept
  {
    static thread_local print_config cfg;
    return cfg;
  }

  namespace detail
  {

    // write — forward declaration
    template <typename T>
    void write(std::ostream &os, const T &value);

    // Indentation helper
    struct indent_guard
    {
      std::ostream &os;
      int level;
      bool compact;
      std::string_view unit;

      void nl() const
      {
        if (!compact)
        {
          os << '\n';
          for (int i = 0; i < level; ++i)
            os << unit;
        }
      }
    };

    // write_bool
    inline void write_bool(std::ostream &os, bool v)
    {
      os << (v ? "true" : "false");
    }

    // write_char
    inline void write_char(std::ostream &os, char c)
    {
      os << '\'' << c << '\'';
    }

    inline void write_char(std::ostream &os, signed char c)
    {
      os << '\'' << static_cast<char>(c) << '\'';
    }

    inline void write_char(std::ostream &os, unsigned char c)
    {
      os << '\'' << static_cast<char>(c) << '\'';
    }

    inline void write_char(std::ostream &os, wchar_t c)
    {
      // Render wchar_t as U+XXXX
      os << "L'\\u" << std::hex << std::uppercase
         << static_cast<unsigned>(c) << std::dec << "'";
    }

    inline void write_char(std::ostream &os, char8_t c)
    {
      os << "u8'" << static_cast<char>(c) << '\'';
    }

    inline void write_char(std::ostream &os, char16_t c)
    {
      os << "u'\\u" << std::hex << std::uppercase
         << static_cast<unsigned>(c) << std::dec << "'";
    }

    inline void write_char(std::ostream &os, char32_t c)
    {
      os << "U'\\U" << std::hex << std::uppercase
         << static_cast<unsigned long>(c) << std::dec << "'";
    }

    // write_nullptr
    inline void write_nullptr(std::ostream &os)
    {
      os << "nullptr";
    }

    // write_string
    inline void write_string(std::ostream &os, std::string_view sv)
    {
      os << '"' << sv << '"';
    }

    inline void write_wstring(std::ostream &os, std::wstring_view wsv)
    {
      // Convert wstring to UTF-8 for display
      os << "L\"";
      for (wchar_t wc : wsv)
      {
        if (wc < 0x80)
        {
          os << static_cast<char>(wc);
        }
        else
        {
          os << "\\u" << std::hex << std::uppercase
             << static_cast<unsigned>(wc) << std::dec;
        }
      }
      os << '"';
    }

    // write_pointer
    template <typename T>
    void write_pointer(std::ostream &os, const T *ptr)
    {
      if (ptr == nullptr)
      {
        os << "nullptr";
      }
      else
      {
        os << "<ptr:0x" << std::hex << std::uppercase
           << reinterpret_cast<std::uintptr_t>(ptr) << std::dec << ">";
      }
    }

    // write_enum
    template <typename E>
      requires std::is_enum_v<E>
    void write_enum(std::ostream &os, E e)
    {
      os << static_cast<std::underlying_type_t<E>>(e);
    }

    // write_duration
    template <typename Rep, typename Period>
    void write_duration(std::ostream &os,
                        const std::chrono::duration<Rep, Period> &d)
    {
      // Attempt to render in the most meaningful unit
      using namespace std::chrono;

      if constexpr (std::is_same_v<Period, std::nano>)
      {
        os << d.count() << "ns";
      }
      else if constexpr (std::is_same_v<Period, std::micro>)
      {
        os << d.count() << "µs";
      }
      else if constexpr (std::is_same_v<Period, std::milli>)
      {
        os << d.count() << "ms";
      }
      else if constexpr (std::ratio_equal_v<Period, std::ratio<1>>)
      {
        os << d.count() << "s";
      }
      else if constexpr (std::ratio_equal_v<Period, std::ratio<60>>)
      {
        os << d.count() << "min";
      }
      else if constexpr (std::ratio_equal_v<Period, std::ratio<3600>>)
      {
        os << d.count() << "h";
      }
      else
      {
        // Generic fallback: show count + ratio
        os << d.count()
           << " [" << Period::num << '/' << Period::den << "]s";
      }
    }

    // write_time_point
    template <typename Clock, typename Duration>
    void write_time_point(std::ostream &os,
                          const std::chrono::time_point<Clock, Duration> &tp)
    {
      // Show as duration since epoch
      os << "<time_point: ";
      write_duration(os, tp.time_since_epoch());
      os << " since epoch>";
    }

    // write_fs_path
    inline void write_fs_path(std::ostream &os,
                              const std::filesystem::path &p)
    {
      os << "path(\"" << p.string() << "\")";
    }

    // write_optional
    template <typename T>
    void write_optional(std::ostream &os, const std::optional<T> &opt)
    {
      if (opt.has_value())
      {
        os << "Some(";
        write(os, *opt);
        os << ')';
      }
      else
      {
        os << "None";
      }
    }

    // write_variant
    template <typename... Ts>
    void write_variant(std::ostream &os, const std::variant<Ts...> &var)
    {
      os << "variant<";
      std::visit([&os](const auto &v)
                 { write(os, v); }, var);
      os << '>';
    }

    // write_any
    inline void write_any(std::ostream &os, const std::any &a)
    {
      if (!a.has_value())
      {
        os << "any(empty)";
      }
      else
      {
        os << "any(" << a.type().name() << ")";
      }
    }

    // write_reference_wrapper
    template <typename T>
    void write_ref_wrapper(std::ostream &os,
                           const std::reference_wrapper<T> &rw)
    {
      os << "ref(";
      write(os, rw.get());
      os << ')';
    }

    // Tuple helpers
    template <typename Tuple, std::size_t... Is>
    void write_tuple_elements(std::ostream &os,
                              const Tuple &t,
                              std::index_sequence<Is...>)
    {
      ((Is == 0 ? void(0) : void(os << ", "),
        write(os, std::get<Is>(t))),
       ...);
    }

    template <typename Tuple>
    void write_tuple(std::ostream &os, const Tuple &t)
    {
      os << '(';
      write_tuple_elements(os, t,
                           std::make_index_sequence<std::tuple_size_v<Tuple>>{});
      os << ')';
    }

    // Pair
    template <typename A, typename B>
    void write_pair(std::ostream &os, const std::pair<A, B> &p)
    {
      os << '(';
      write(os, p.first);
      os << ": ";
      write(os, p.second);
      os << ')';
    }

    // Map key-value pair
    template <typename A, typename B>
    void write_kv_pair(std::ostream &os, const std::pair<const A, B> &kv)
    {
      write(os, kv.first);
      os << " => ";
      write(os, kv.second);
    }

    // Range rendering
    template <typename Range>
    void write_range(
        std::ostream &os,
        const Range &rng,
        char open, char close,
        std::size_t max_items,
        bool is_map = false)
    {
      os << open;
      std::size_t count = 0;
      bool first = true;
      for (const auto &elem : rng)
      {
        if (count >= max_items)
        {
          os << ", ...";
          break;
        }
        if (!first)
          os << ", ";
        first = false;
        if constexpr (traits::is_map_like_v<Range>)
        {
          write_kv_pair(os, elem);
        }
        else
        {
          write(os, elem);
        }
        ++count;
      }
      os << close;
      (void)is_map;
    }

    //  write_smart_ptr
    template <typename T, typename Deleter>
    void write_unique_ptr(std::ostream &os,
                          const std::unique_ptr<T, Deleter> &p)
    {
      if (!p)
      {
        os << "unique_ptr(null)";
      }
      else
      {
        os << "unique_ptr(";
        write(os, *p);
        os << ')';
      }
    }

    template <typename T>
    void write_shared_ptr(std::ostream &os, const std::shared_ptr<T> &p)
    {
      if (!p)
      {
        os << "shared_ptr(null)";
      }
      else
      {
        os << "shared_ptr[use=" << p.use_count() << "](";
        write(os, *p);
        os << ')';
      }
    }

    template <typename T>
    void write_weak_ptr(std::ostream &os, const std::weak_ptr<T> &p)
    {
      if (p.expired())
      {
        os << "weak_ptr(expired)";
      }
      else if (auto sp = p.lock())
      {
        os << "weak_ptr[use=" << sp.use_count() << "](";
        write(os, *sp);
        os << ')';
      }
      else
      {
        os << "weak_ptr(null)";
      }
    }

    // Container adapter helpers (copy-drain approach)
    template <typename T, typename Container>
    void write_stack(
        std::ostream &os,
        std::stack<T, Container> s, // by value — intentional copy
        std::size_t max_items)
    {
      os << "stack[";
      std::vector<T> items;
      items.reserve(s.size());
      while (!s.empty())
      {
        items.push_back(s.top());
        s.pop();
      }
      std::reverse(items.begin(), items.end());
      std::size_t count = 0;
      bool first = true;
      for (const auto &v : items)
      {
        if (count++ >= max_items)
        {
          os << ", ...";
          break;
        }
        if (!first)
          os << ", ";
        first = false;
        write(os, v);
      }
      os << "]";
    }

    template <typename T, typename Container>
    void write_queue(
        std::ostream &os,
        std::queue<T, Container> q, // by value — intentional copy
        std::size_t max_items)
    {
      os << "queue[";
      std::size_t count = 0;
      bool first = true;
      while (!q.empty())
      {
        if (count++ >= max_items)
        {
          os << ", ...";
          break;
        }
        if (!first)
          os << ", ";
        first = false;
        write(os, q.front());
        q.pop();
      }
      os << "]";
    }

    template <typename T, typename Container, typename Cmp>
    void write_priority_queue(
        std::ostream &os,
        std::priority_queue<T, Container, Cmp> pq,
        std::size_t max_items)
    {
      os << "priority_queue[";
      std::size_t count = 0;
      bool first = true;
      while (!pq.empty())
      {
        if (count++ >= max_items)
        {
          os << ", ...";
          break;
        }
        if (!first)
          os << ", ";
        first = false;
        write(os, pq.top());
        pq.pop();
      }
      os << "]";
    }

#if VIX_HAS_EXPECTED
    // write_expected
    template <typename T, typename E>
    void write_expected(std::ostream &os, const std::expected<T, E> &exp)
    {
      if (exp.has_value())
      {
        os << "Ok(";
        write(os, *exp);
        os << ')';
      }
      else
      {
        os << "Err(";
        write(os, exp.error());
        os << ')';
      }
    }
#endif

    // vix::formatter detection
    template <typename T, typename = void>
    struct has_formatter_specialization : std::false_type
    {
    };

    template <typename T>
    struct has_formatter_specialization<T,
                                        std::void_t<decltype(vix::formatter<T>::format(
                                            std::declval<std::ostream &>(),
                                            std::declval<const T &>()))>>
        : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool has_formatter_v =
        has_formatter_specialization<std::remove_cvref_t<T>>::value;

    /**
     * @brief Central dispatch function — routes any type T to the correct renderer.
     *
     * Priority order:
     *  1. vix::formatter<T> specialization
     *  2. ADL vix_format hook
     *  3. nullptr_t
     *  4. bool
     *  5. char types
     *  6. wstring / wstring_view
     *  7. string-like types
     *  8. std::filesystem::path
     *  9. std::any
     * 10. std::optional<T>
     * 11. std::variant<Ts...>
     * 12. std::reference_wrapper<T>
     * 13. chrono duration
     * 14. chrono time_point
     * 15. smart pointers
     * 16. container adapters
     * 17. std::expected (C++23)
     * 18. enum types
     * 19. map-like ranges
     * 20. set/sequence ranges
     * 21. tuple-like (pair, tuple, array)
     * 22. raw pointer
     * 23. function pointer
     * 24. streamable via operator<<
     * 25. fallback: type name
     */
    template <typename T>
    void write(std::ostream &os, const T &value)
    {
      using U = std::remove_cvref_t<T>;

      // User formatter specialization
      if constexpr (has_formatter_v<U>)
      {
        vix::formatter<U>::format(os, value);
      }
      // ADL vix_format hook
      else if constexpr (traits::has_vix_format_v<U>)
      {
        vix_format(os, value);
      }
      // nullptr_t
      else if constexpr (traits::is_nullptr_v<U>)
      {
        write_nullptr(os);
      }
      // bool
      else if constexpr (traits::is_bool_v<U>)
      {
        write_bool(os, value);
      }
      // char types
      else if constexpr (traits::is_char_v<U>)
      {
        write_char(os, value);
      }
      // wstring / wstring_view / const wchar_t*
      else if constexpr (std::is_same_v<U, std::wstring> ||
                         std::is_same_v<U, std::wstring_view>)
      {
        write_wstring(os, value);
      }
      else if constexpr (std::is_same_v<U, const wchar_t *> ||
                         std::is_same_v<U, wchar_t *>)
      {
        if (value == nullptr)
          write_nullptr(os);
        else
          write_wstring(os, std::wstring_view{value});
      }
      // String-like
      else if constexpr (traits::is_string_like_v<U>)
      {
        write_string(os, value);
      }
      // filesystem::path
      else if constexpr (traits::is_fs_path_v<U>)
      {
        write_fs_path(os, value);
      }
      // std::any
      else if constexpr (traits::is_any_v<U>)
      {
        write_any(os, value);
      }
      // std::optional
      else if constexpr (traits::is_optional_v<U>)
      {
        write_optional(os, value);
      }
      // std::variant
      else if constexpr (traits::is_variant_v<U>)
      {
        write_variant(os, value);
      }
      // std::reference_wrapper
      else if constexpr (traits::is_reference_wrapper_v<U>)
      {
        write_ref_wrapper(os, value);
      }
      // chrono duration
      else if constexpr (traits::is_duration_v<U>)
      {
        write_duration(os, value);
      }
      // chrono time_point
      else if constexpr (traits::is_time_point_v<U>)
      {
        write_time_point(os, value);
      }
      // smart pointers
      else if constexpr (traits::is_unique_ptr<U>::value)
      {
        write_unique_ptr(os, value);
      }
      else if constexpr (traits::is_shared_ptr<U>::value)
      {
        write_shared_ptr(os, value);
      }
      else if constexpr (traits::is_weak_ptr<U>::value)
      {
        write_weak_ptr(os, value);
      }
      // Container adapters
      else if constexpr (traits::is_stack<U>::value)
      {
        write_stack(os, value, default_config().max_items);
      }
      else if constexpr (traits::is_queue<U>::value)
      {
        write_queue(os, value, default_config().max_items);
      }
      else if constexpr (traits::is_priority_queue<U>::value)
      {
        write_priority_queue(os, value, default_config().max_items);
      }
#if VIX_HAS_EXPECTED
      // std::expected
      else if constexpr (traits::is_expected_v<U>)
      {
        write_expected(os, value);
      }
#endif
      // enum
      else if constexpr (traits::is_enum_v<U>)
      {
        write_enum(os, value);
      }
      // map-like ranges
      else if constexpr (traits::is_map_like_v<U> && traits::is_range_v<U>)
      {
        write_range(os, value, '{', '}', default_config().max_items, true);
      }
      // other ranges (vector, list, set, ...)
      else if constexpr (traits::is_range_v<U>)
      {
        write_range(os, value, '[', ']', default_config().max_items, false);
      }
      // tuple-like (pair, tuple, std::array of non-range)
      else if constexpr (traits::is_pair_v<U>)
      {
        write_pair(os, value);
      }
      else if constexpr (traits::is_tuple_like_v<U>)
      {
        write_tuple(os, value);
      }
      // raw pointer
      else if constexpr (std::is_pointer_v<U> && !traits::is_string_like_v<U>)
      {
        if constexpr (std::is_function_v<std::remove_pointer_t<U>>)
        {
          // function pointer: show address
          os << "<fptr:0x" << std::hex << std::uppercase
             << reinterpret_cast<std::uintptr_t>(
                    reinterpret_cast<const void *>(value))
             << std::dec << ">";
        }
        else
        {
          write_pointer(os, value);
        }
      }
      else if constexpr (traits::is_ostreamable_v<U>)
      {
        os << value;
      }
      else
      {
        os << "<unprintable:" << typeid(U).name() << ">";
      }
    }

  } // namespace detail

  /**
   * @brief Specialize this template to teach vix::print how to render your type.
   *
   * @example
   *   template<> struct vix::formatter<MyType> {
   *       static void format(std::ostream& os, const MyType& v) {
   *           os << "MyType{" << v.x << ", " << v.y << "}";
   *       }
   *   };
   */
  template <typename T, typename>
  struct formatter
  {
    // No default implementation — detected at compile time.
  };

  /**
   * @brief Write a single value to an ostream using the vix rendering engine.
   * @param os    Destination stream.
   * @param value Value to render.
   */
  template <typename T>
  void write_to(std::ostream &os, const T &value)
  {
    detail::write(os, value);
  }

  /**
   * @brief Format a single value to a std::string.
   * @param value Value to render.
   * @return Rendered string representation.
   */
  template <typename T>
  [[nodiscard]] std::string to_string(const T &value)
  {
    std::ostringstream oss;
    detail::write(oss, value);
    return oss.str();
  }

  namespace detail
  {

    template <typename... Args>
    void print_impl(const print_config &cfg, const Args &...args)
    {
      std::ostream &os = *cfg.out;
      bool first = true;
      ((
           [&]()
           {
             if (!first)
               os << cfg.separator;
             first = false;
             write(os, args);
           }()),
       ...);
      os << cfg.end;
    }

  } // namespace detail

  /**
   * @brief Print any number of values to stdout, separated by spaces.
   *
   * Behaviour mirrors Python's print():
   *   - Arguments are separated by a space by default.
   *   - A newline is appended at the end by default.
   *   - Every type supported by the vix engine is rendered intelligently.
   *
   * @param args Zero or more values of any supported type.
   *
   * @example
   *   vix::print(1, "hello", std::vector{1,2,3}, std::nullopt);
   *   // Output: 1 "hello" [1, 2, 3] None
   */
  template <typename... Args>
  void print(const Args &...args)
  {
    detail::print_impl(default_config(), args...);
  }

  /**
   * @brief Print with a custom configuration.
   * @param cfg   Rendering configuration.
   * @param args  Values to print.
   */
  template <typename... Args>
  void print(const print_config &cfg, const Args &...args)
  {
    detail::print_impl(cfg, args...);
  }

  /**
   * @brief Print to a specific stream.
   * @param os    Destination stream.
   * @param args  Values to print.
   */
  template <typename... Args>
  void print_to(std::ostream &os, const Args &...args)
  {
    print_config cfg = default_config();
    cfg.out = &os;
    detail::print_impl(cfg, args...);
  }

  /**
   * @brief Print to stderr.
   * @param args  Values to print.
   */
  template <typename... Args>
  void eprint(const Args &...args)
  {
    print_config cfg = default_config();
    cfg.out = &std::cerr;
    detail::print_impl(cfg, args...);
  }

  /**
   * @brief Print without a trailing newline.
   * @param args  Values to print.
   */
  template <typename... Args>
  void print_inline(const Args &...args)
  {
    print_config cfg = default_config();
    cfg.end = "";
    detail::print_impl(cfg, args...);
  }

  /**
   * @brief Format arguments to a std::string (no output written).
   * @param args  Values to format.
   * @return      Formatted string.
   */
  template <typename... Args>
  [[nodiscard]] std::string sprint(const Args &...args)
  {
    std::ostringstream oss;
    print_config cfg = default_config();
    cfg.out = &oss;
    cfg.end = "";
    detail::print_impl(cfg, args...);
    return oss.str();
  }

  /**
   * @brief Print a blank line (equivalent to Python's print()).
   */
  inline void print()
  {
    *default_config().out << default_config().end;
  }

  /**
   * @brief Print a named value: `label: value`.
   * @param label  Label string.
   * @param value  Any printable value.
   */
  template <typename T>
  void print_named(std::string_view label, const T &value)
  {
    auto &os = *default_config().out;
    os << label << ": ";
    detail::write(os, value);
    os << default_config().end;
  }

  /**
   * @brief Print a horizontal separator line.
   * @param width  Line width (default 60).
   * @param ch     Fill character (default '─').
   */
  inline void print_separator(std::size_t width = 60, char ch = '-')
  {
    auto &os = *default_config().out;
    for (std::size_t i = 0; i < width; ++i)
      os << ch;
    os << '\n';
  }

  /**
   * @brief Print a titled section header.
   * @param title  Section title.
   * @param width  Total width.
   */
  inline void print_header(std::string_view title,
                           std::size_t width = 60)
  {
    auto &os = *default_config().out;
    print_separator(width);
    std::size_t padding = (width > title.size() + 2)
                              ? (width - title.size() - 2) / 2
                              : 0;
    for (std::size_t i = 0; i < padding; ++i)
      os << ' ';
    os << ' ' << title << ' ';
    os << '\n';
    print_separator(width);
  }

  /**
   * @brief Print each element of a range on its own line, optionally with index.
   * @param rng          Iterable range.
   * @param show_index   Prefix each line with its zero-based index.
   */
  template <typename Range>
    requires traits::is_range_v<Range>
  void print_each(const Range &rng, bool show_index = false)
  {
    auto &os = *default_config().out;
    std::size_t idx = 0;
    for (const auto &elem : rng)
    {
      if (show_index)
        os << '[' << idx++ << "] ";
      detail::write(os, elem);
      os << '\n';
    }
  }

  /**
   * @brief Print a map as a two-column table.
   * @param m           Map-like container.
   * @param col_width   Width of the key column.
   */
  template <typename Map>
    requires traits::is_map_like_v<Map>
  void print_table(const Map &m, std::size_t col_width = 20)
  {
    auto &os = *default_config().out;
    print_separator(col_width * 2 + 3);
    for (const auto &[k, v] : m)
    {
      std::ostringstream ks, vs;
      detail::write(ks, k);
      detail::write(vs, v);
      std::string ks_str = ks.str();
      std::string vs_str = vs.str();
      os << ks_str;
      std::size_t pad = (col_width > ks_str.size())
                            ? col_width - ks_str.size()
                            : 1;
      for (std::size_t i = 0; i < pad; ++i)
        os << ' ';
      os << "| " << vs_str << '\n';
    }
    print_separator(col_width * 2 + 3);
  }

  /**
   * @brief RAII guard that temporarily overrides the default print_config.
   *
   * @example
   *   {
   *       vix::scoped_config guard{};
   *       guard.cfg.separator = ", ";
   *       guard.cfg.end = ";\n";
   *       vix::print(1, 2, 3);  // prints: 1, 2, 3;
   *   }
   *   // original config restored
   */
  class scoped_config
  {
  public:
    print_config cfg;

    explicit scoped_config(print_config override = default_config())
        : cfg(std::move(override)), saved_(default_config())
    {
      default_config() = cfg;
    }

    ~scoped_config()
    {
      default_config() = saved_;
    }

    scoped_config(const scoped_config &) = delete;
    scoped_config &operator=(const scoped_config &) = delete;

  private:
    print_config saved_;
  };

  /**
   * @brief Convenience base for formatter specializations using operator<<.
   *
   * Inherit from this in a formatter specialization to delegate to operator<<:
   *
   *   template<> struct vix::formatter<MyType>
   *       : vix::streamable_formatter<MyType> {};
   */
  template <typename T>
  struct streamable_formatter
  {
    static void format(std::ostream &os, const T &v)
    {
      os << v;
    }
  };

  // std::monostate
  template <>
  struct formatter<std::monostate>
  {
    static void format(std::ostream &os, const std::monostate &)
    {
      os << "monostate";
    }
  };

  // std::byte
  template <>
  struct formatter<std::byte>
  {
    static void format(std::ostream &os, const std::byte &b)
    {
      os << "0x" << std::hex << std::uppercase
         << static_cast<unsigned>(b) << std::dec;
    }
  };

  // std::bitset<N>
  // Generic via operator<< detection — no special case needed.

  // std::complex<T>
  // operator<< exists for std::complex — handled automatically.

  // std::error_code
  template <>
  struct formatter<std::error_code>
  {
    static void format(std::ostream &os, const std::error_code &ec)
    {
      os << "error_code(" << ec.value()
         << ", \"" << ec.message() << "\")";
    }
  };

  template <>
  struct formatter<std::error_condition>
  {
    static void format(std::ostream &os, const std::error_condition &ec)
    {
      os << "error_condition(" << ec.value()
         << ", \"" << ec.message() << "\")";
    }
  };

  /**
   * @brief Returns a human-readable description of how vix would render type T.
   *
   * Useful during development to understand the rendering path chosen.
   */
  template <typename T>
  [[nodiscard]] std::string_view rendering_path() noexcept
  {
    using U = std::remove_cvref_t<T>;

    if constexpr (detail::has_formatter_v<U>)
      return "vix::formatter<T> specialization";
    else if constexpr (traits::has_vix_format_v<U>)
      return "ADL vix_format hook";
    else if constexpr (traits::is_nullptr_v<U>)
      return "nullptr_t";
    else if constexpr (traits::is_bool_v<U>)
      return "bool";
    else if constexpr (traits::is_char_v<U>)
      return "char type";
    else if constexpr (traits::is_string_like_v<U>)
      return "string-like";
    else if constexpr (traits::is_fs_path_v<U>)
      return "filesystem::path";
    else if constexpr (traits::is_any_v<U>)
      return "std::any";
    else if constexpr (traits::is_optional_v<U>)
      return "std::optional";
    else if constexpr (traits::is_variant_v<U>)
      return "std::variant";
    else if constexpr (traits::is_reference_wrapper_v<U>)
      return "std::reference_wrapper";
    else if constexpr (traits::is_duration_v<U>)
      return "chrono::duration";
    else if constexpr (traits::is_time_point_v<U>)
      return "chrono::time_point";
    else if constexpr (traits::is_unique_ptr<U>::value)
      return "std::unique_ptr";
    else if constexpr (traits::is_shared_ptr<U>::value)
      return "std::shared_ptr";
    else if constexpr (traits::is_weak_ptr<U>::value)
      return "std::weak_ptr";
    else if constexpr (traits::is_stack<U>::value)
      return "std::stack";
    else if constexpr (traits::is_queue<U>::value)
      return "std::queue";
    else if constexpr (traits::is_priority_queue<U>::value)
      return "std::priority_queue";
#if VIX_HAS_EXPECTED
    else if constexpr (traits::is_expected_v<U>)
      return "std::expected";
#endif
    else if constexpr (traits::is_enum_v<U>)
      return "enum";
    else if constexpr (traits::is_map_like_v<U> && traits::is_range_v<U>)
      return "map-like range";
    else if constexpr (traits::is_range_v<U>)
      return "range";
    else if constexpr (traits::is_pair_v<U>)
      return "std::pair";
    else if constexpr (traits::is_tuple_like_v<U>)
      return "tuple-like";
    else if constexpr (std::is_pointer_v<U>)
      return "raw pointer";
    else if constexpr (traits::is_ostreamable_v<U>)
      return "operator<< (streamable)";
    else
      return "fallback (unprintable)";
  }

  /**
   * @brief Print the rendering path for each type in a pack.
   */
  template <typename... Ts>
  void print_rendering_paths()
  {
    auto &os = *default_config().out;
    ((os << typeid(Ts).name() << " => "
         << rendering_path<Ts>() << '\n'),
     ...);
  }

#ifdef VIX_ENABLE_DEMO

  namespace demo
  {
    struct Point2D
    {
      double x, y;
    };

    // Extension via vix_format ADL hook
    inline void vix_format(std::ostream &os, const Point2D &p)
    {
      os << "Point2D{x=" << p.x << ", y=" << p.y << '}';
    }

    struct Color
    {
      uint8_t r, g, b, a = 255;
    };

    // Extension via vix::formatter specialization (must be in vix namespace)
  } // namespace demo
} // namespace vix

template <>
struct vix::formatter<vix::demo::Color>
{
  static void format(std::ostream &os, const vix::demo::Color &c)
  {
    os << "Color(r=" << +c.r << ", g=" << +c.g
       << ", b=" << +c.b << ", a=" << +c.a << ')';
  }
};

namespace vix
{
  namespace demo
  {

    struct NamedValue
    {
      std::string name;
      int value;
    };
    // Extension via operator<< (auto-detected)
    inline std::ostream &operator<<(std::ostream &os, const NamedValue &nv)
    {
      return os << nv.name << '=' << nv.value;
    }

    void demo_primitives()
    {
      vix::print_header("Primitives");
      vix::print(true, false);
      vix::print('A', '\n', '\t');
      vix::print(42, -17, 0u);
      vix::print(3.14f, 2.718281828, 1.41421356237L);
      vix::print(nullptr);
      vix::print(std::numeric_limits<int>::max(),
                 std::numeric_limits<double>::infinity());
    }

    void demo_strings()
    {
      vix::print_header("Strings");
      vix::print("hello world");
      vix::print(std::string{"std::string value"});
      vix::print(std::string_view{"string_view value"});
      const char *raw = "raw const char*";
      vix::print(raw);
      wchar_t wraw[] = L"wide string";
      vix::print(std::wstring{wraw});
    }

    void demo_pointers()
    {
      vix::print_header("Pointers");
      int x = 42;
      int *px = &x;
      int *pnull = nullptr;
      vix::print(px, pnull);
      void (*fptr)(int) = [](int) {};
      vix::print(fptr);
    }

    void demo_enums()
    {
      vix::print_header("Enums");
      enum Color
      {
        Red = 1,
        Green,
        Blue
      };
      enum class Direction
      {
        North = 0,
        South,
        East,
        West
      };
      vix::print(Red, Green, Blue);
      vix::print(Direction::North, Direction::East);
    }

    void demo_stl_sequential()
    {
      vix::print_header("STL Sequential Containers");

      vix::print(std::vector<int>{1, 2, 3, 4, 5});
      vix::print(std::array<double, 3>{1.1, 2.2, 3.3});
      vix::print(std::deque<std::string>{"a", "b", "c"});
      vix::print(std::list<int>{10, 20, 30});
      vix::print(std::forward_list<int>{7, 8, 9});
    }

    void demo_stl_associative()
    {
      vix::print_header("STL Associative Containers");

      vix::print(std::set<int>{1, 2, 3, 4});
      vix::print(std::multiset<int>{1, 1, 2, 3});
      vix::print(std::map<std::string, int>{{"a", 1}, {"b", 2}, {"c", 3}});
      vix::print(std::multimap<int, std::string>{{1, "x"}, {1, "y"}, {2, "z"}});
      vix::print(std::unordered_set<int>{10, 20, 30});
      vix::print(std::unordered_map<std::string, double>{{"pi", 3.14}});
    }

    void demo_adapters()
    {
      vix::print_header("Container Adapters");

      std::stack<int> st;
      for (int i : {1, 2, 3})
        st.push(i);
      vix::print(st);

      std::queue<std::string> q;
      for (auto &s : {"first", "second", "third"})
        q.push(s);
      vix::print(q);

      std::priority_queue<int> pq;
      for (int i : {5, 1, 4, 2, 3})
        pq.push(i);
      vix::print(pq);
    }

    void demo_utilities()
    {
      vix::print_header("Utility Types");

      vix::print(std::make_pair(42, "hello"));
      vix::print(std::make_tuple(1, 2.0, "three", true));
      vix::print(std::optional<int>{42});
      vix::print(std::optional<int>{});
      vix::print(std::make_optional(std::string{"optional string"}));

      std::variant<int, double, std::string> v1 = 42;
      std::variant<int, double, std::string> v2 = 3.14;
      std::variant<int, double, std::string> v3 = std::string{"variant str"};
      vix::print(v1, v2, v3);

      std::any a1 = 42;
      std::any a2 = std::string{"any string"};
      std::any a3;
      vix::print(a1, a2, a3);

      int x = 99;
      vix::print(std::ref(x), std::cref(x));
    }

    void demo_smart_pointers()
    {
      vix::print_header("Smart Pointers");

      auto up1 = std::make_unique<int>(42);
      std::unique_ptr<int> up2;
      vix::print(up1, up2);

      auto sp1 = std::make_shared<std::string>("shared value");
      std::shared_ptr<int> sp2;
      auto sp3 = std::make_shared<int>(100);
      auto sp4 = sp3; // use_count = 2
      vix::print(sp1, sp2, sp3, sp4);

      std::weak_ptr<int> wp1 = sp3;
      std::weak_ptr<int> wp2;
      vix::print(wp1, wp2);
    }

    void demo_chrono()
    {
      vix::print_header("Chrono");

      using namespace std::chrono;
      vix::print(42ns, 100us, 1500ms, 60s, 2min, 1h);

      auto now = system_clock::now();
      vix::print(now);

      auto since = duration_cast<seconds>(now.time_since_epoch());
      vix::print(since);
    }

    void demo_filesystem()
    {
      vix::print_header("Filesystem");

      namespace fs = std::filesystem;
      vix::print(fs::path{"/usr/local/include"});
      vix::print(fs::path{"relative/path/file.txt"});
      vix::print(fs::path{});
    }

    void demo_nested_types()
    {
      vix::print_header("Nested / Complex Types");

      // vector<map<string, int>>
      std::vector<std::map<std::string, int>> vmap = {
          {{"a", 1}, {"b", 2}},
          {{"x", 10}, {"y", 20}}};
      vix::print(vmap);

      // map<string, vector<int>>
      std::map<std::string, std::vector<int>> mvi = {
          {"evens", {2, 4, 6}},
          {"odds", {1, 3, 5}}};
      vix::print(mvi);

      // tuple<string, vector<pair<int,int>>, optional<double>>
      auto t = std::make_tuple(
          std::string{"label"},
          std::vector<std::pair<int, int>>{{1, 2}, {3, 4}},
          std::optional<double>{3.14});
      vix::print(t);

      // optional<vector<variant<int,string>>>
      using Elem = std::variant<int, std::string>;
      std::optional<std::vector<Elem>> ovv = std::vector<Elem>{
          42, std::string{"hello"}, 7};
      vix::print(ovv);

      // map<string, shared_ptr<vector<int>>>
      auto v1 = std::make_shared<std::vector<int>>(std::initializer_list<int>{1, 2, 3});
      auto v2 = std::make_shared<std::vector<int>>(std::initializer_list<int>{4, 5, 6});
      std::map<std::string, std::shared_ptr<std::vector<int>>> mspv{
          {"first", v1}, {"second", v2}};
      vix::print(mspv);
    }

    void demo_user_types()
    {
      vix::print_header("User-Defined Types");

      Point2D p{1.5, -2.7};
      vix::print(p);

      Color red{255, 0, 0};
      Color transparent{0, 0, 0, 0};
      vix::print(red, transparent);

      NamedValue nv{"answer", 42};
      vix::print(nv);

      // vector of user types
      vix::print(std::vector<Point2D>{{0, 0}, {1, 1}, {2, 4}});
    }

    void demo_sprint_and_config()
    {
      vix::print_header("sprint / config");

      std::string s = vix::sprint(42, "hello", std::vector{1, 2, 3});
      vix::print_named("sprint result", s);

      {
        vix::scoped_config g{};
        g.cfg.separator = " | ";
        g.cfg.end = " [END]\n";
        vix::print(1, 2, 3);
      }

      // print_each with index
      vix::print_each(std::vector<std::string>{"alpha", "beta", "gamma"}, true);

      // print_table
      vix::print_table(std::map<std::string, int>{{"foo", 1}, {"bar", 2}, {"baz", 3}});
    }

    void demo_byte_and_special()
    {
      vix::print_header("std::byte / error_code / monostate");

      vix::print(std::byte{0xDE}, std::byte{0xAD});

      std::error_code ec = std::make_error_code(std::errc::permission_denied);
      vix::print(ec);

      std::variant<std::monostate, int, std::string> mv;
      vix::print(mv); // monostate
      mv = 42;
      vix::print(mv);
      mv = std::string{"active"};
      vix::print(mv);
    }

    void demo_rendering_paths()
    {
      vix::print_header("Rendering Paths Diagnostic");
      vix::print_rendering_paths<
          int, bool, char, std::string, std::string_view,
          std::vector<int>, std::map<std::string, int>,
          std::optional<int>, std::variant<int, std::string>,
          std::unique_ptr<int>, std::shared_ptr<double>,
          std::chrono::seconds, std::filesystem::path,
          std::byte, std::error_code,
          Point2D, Color, NamedValue>();
    }

    /**
     * @brief Run all demo sections.
     */
    void run_all()
    {
      demo_primitives();
      print();
      demo_strings();
      print();
      demo_pointers();
      print();
      demo_enums();
      print();
      demo_stl_sequential();
      print();
      demo_stl_associative();
      print();
      demo_adapters();
      print();
      demo_utilities();
      print();
      demo_smart_pointers();
      print();
      demo_chrono();
      print();
      demo_filesystem();
      print();
      demo_nested_types();
      print();
      demo_user_types();
      print();
      demo_sprint_and_config();
      print();
      demo_byte_and_special();
      print();
      demo_rendering_paths();
    }

  } // namespace demo

#ifdef VIX_MAIN

  int run_demo()
  {
    demo::run_all();
    return 0;
  }

  int main()
  {
    return run_demo();
  }

#endif // VIX_MAIN
#endif // VIX_ENABLE_DEMO

  namespace pretty
  {

    /**
     * @brief Context carried through the recursive pretty-print traversal.
     */
    struct context
    {
      std::ostream &os;
      int depth = 0;
      int max_depth = 8;
      std::size_t max_items = 32;
      std::string indent_str = "  ";
      bool color = false;

      void indent() const
      {
        for (int i = 0; i < depth; ++i)
          os << indent_str;
      }
      void newline() const { os << '\n'; }
    };

    // Forward declaration
    template <typename T>
    void pwrite(context &ctx, const T &value);

    template <typename T>
    void pwrite_scalar(context &ctx, const T &v)
    {
      detail::write(ctx.os, v);
    }

    template <typename Range>
      requires traits::is_range_v<Range>
    void pwrite_range(context &ctx, const Range &rng, char open, char close)
    {
      ctx.os << open;
      std::size_t count = 0;

      for (const auto &elem : rng)
      {
        if (count >= ctx.max_items)
        {
          ctx.newline();
          ctx.indent();
          for (int i = 0; i < ctx.depth + 1; ++i)
            ctx.os << ctx.indent_str;
          ctx.os << "...";
          break;
        }

        ctx.newline();
        for (int i = 0; i <= ctx.depth; ++i)
          ctx.os << ctx.indent_str;

        if constexpr (traits::is_map_like_v<Range>)
        {
          detail::write(ctx.os, elem.first);
          ctx.os << " => ";

          context inner{
              ctx.os,
              ctx.depth + 1,
              ctx.max_depth,
              ctx.max_items,
              ctx.indent_str,
              ctx.color};

          pwrite(inner, elem.second);
        }
        else
        {
          context inner{
              ctx.os,
              ctx.depth + 1,
              ctx.max_depth,
              ctx.max_items,
              ctx.indent_str,
              ctx.color};

          pwrite(inner, elem);
        }

        ++count;
      }

      if (count > 0)
      {
        ctx.newline();
        ctx.indent();
      }

      ctx.os << close;
    }

    template <typename Tuple, std::size_t... Is>
    void pwrite_tuple_elems(
        context &ctx, const Tuple &t,
        std::index_sequence<Is...>)
    {
      ((
           [&]()
           {
             ctx.newline();
             for (int i = 0; i <= ctx.depth; ++i)
               ctx.os << ctx.indent_str;
             context inner{ctx.os, ctx.depth + 1, ctx.max_depth,
                           ctx.max_items, ctx.indent_str, ctx.color};
             pwrite(inner, std::get<Is>(t));
           }()),
       ...);
    }

    template <typename Tuple>
    void pwrite_tuple(context &ctx, const Tuple &t)
    {
      ctx.os << '(';
      pwrite_tuple_elems(ctx, t, std::make_index_sequence<std::tuple_size_v<Tuple>>{});
      ctx.newline();
      ctx.indent();
      ctx.os << ')';
    }

    template <typename T>
    void pwrite_optional(context &ctx, const std::optional<T> &opt)
    {
      if (!opt.has_value())
      {
        ctx.os << "None";
        return;
      }
      ctx.os << "Some(";
      context inner{ctx.os, ctx.depth + 1, ctx.max_depth,
                    ctx.max_items, ctx.indent_str, ctx.color};
      if constexpr (traits::is_range_v<T> || traits::is_tuple_like_v<T>)
      {
        ctx.newline();
        for (int i = 0; i <= ctx.depth; ++i)
          ctx.os << ctx.indent_str;
        pwrite(inner, *opt);
        ctx.newline();
        ctx.indent();
      }
      else
      {
        pwrite(inner, *opt);
      }
      ctx.os << ')';
    }

    template <typename... Ts>
    void pwrite_variant(context &ctx, const std::variant<Ts...> &var)
    {
      ctx.os << "variant<";
      std::visit([&ctx](const auto &v)
                 {
        context inner{ctx.os, ctx.depth, ctx.max_depth,
                      ctx.max_items, ctx.indent_str, ctx.color};
        pwrite(inner, v); }, var);
      ctx.os << '>';
    }

    template <typename T, typename D>
    void pwrite_unique(context &ctx, const std::unique_ptr<T, D> &p)
    {
      if (!p)
      {
        ctx.os << "unique_ptr(null)";
        return;
      }
      ctx.os << "unique_ptr(";
      context inner{ctx.os, ctx.depth + 1, ctx.max_depth,
                    ctx.max_items, ctx.indent_str, ctx.color};
      pwrite(inner, *p);
      ctx.os << ')';
    }

    template <typename T>
    void pwrite_shared(context &ctx, const std::shared_ptr<T> &p)
    {
      if (!p)
      {
        ctx.os << "shared_ptr(null)";
        return;
      }
      ctx.os << "shared_ptr[use=" << p.use_count() << "](";
      context inner{ctx.os, ctx.depth + 1, ctx.max_depth,
                    ctx.max_items, ctx.indent_str, ctx.color};
      pwrite(inner, *p);
      ctx.os << ')';
    }

    template <typename T>
    void pwrite(context &ctx, const T &value)
    {
      if (ctx.depth >= ctx.max_depth)
      {
        ctx.os << "<depth-limit>";
        return;
      }

      using U = std::remove_cvref_t<T>;

      if constexpr (traits::is_map_like_v<U> && traits::is_range_v<U>)
      {
        pwrite_range(ctx, value, '{', '}');
      }
      else if constexpr (traits::is_range_v<U>)
      {
        pwrite_range(ctx, value, '[', ']');
      }
      else if constexpr (traits::is_tuple_like_v<U> && !traits::is_range_v<U>)
      {
        pwrite_tuple(ctx, value);
      }
      else if constexpr (traits::is_optional_v<U>)
      {
        pwrite_optional(ctx, value);
      }
      else if constexpr (traits::is_variant_v<U>)
      {
        pwrite_variant(ctx, value);
      }
      else if constexpr (traits::is_unique_ptr<U>::value)
      {
        pwrite_unique(ctx, value);
      }
      else if constexpr (traits::is_shared_ptr<U>::value)
      {
        pwrite_shared(ctx, value);
      }
      else
      {
        pwrite_scalar(ctx, value);
      }
    }

  } // namespace pretty

  /**
   * @brief Multi-line pretty-print with full indentation.
   *
   * Unlike vix::print which renders on a single line,
   * vix::pprint formats nested structures across multiple lines.
   *
   * @param value   Value to pretty-print.
   * @param cfg     Optional config override.
   */
  template <typename T>
  void pprint(const T &value,
              const print_config &cfg = default_config())
  {
    pretty::context ctx{*cfg.out};
    pretty::pwrite(ctx, value);
    *cfg.out << cfg.end;
  }

  /**
   * @brief Multi-line pretty-print for multiple values.
   */
  template <typename... Args>
  void pprint_all(const Args &...args)
  {
    auto &os = *default_config().out;
    ([&]()
     {
        pretty::context ctx2{os};
        pretty::pwrite(ctx2, args);
        os << '\n'; }(), ...);
  }

  /**
   * @brief Print statistics for a numeric range (min, max, sum, average).
   *
   * @param rng   Range of arithmetic values.
   * @param label Optional label.
   */
  template <typename Range>
    requires traits::is_range_v<Range> &&
             std::is_arithmetic_v<
                 std::remove_cvref_t<decltype(*std::begin(std::declval<Range>()))>>
  void print_stats(const Range &rng, std::string_view label = "")
  {
    using V = std::remove_cvref_t<decltype(*std::begin(rng))>;
    auto &os = *default_config().out;

    if (label.size())
      os << label << ": ";

    if (std::begin(rng) == std::end(rng))
    {
      os << "stats(empty)\n";
      return;
    }

    V mn = *std::begin(rng);
    V mx = mn;
    double sum = 0.0;
    std::size_t n = 0;

    for (const auto &v : rng)
    {
      if (v < mn)
        mn = v;
      if (v > mx)
        mx = v;
      sum += static_cast<double>(v);
      ++n;
    }

    os << "stats{n=" << n
       << ", min=" << mn
       << ", max=" << mx
       << ", sum=" << sum
       << ", avg=" << (n ? sum / n : 0.0)
       << "}\n";
  }

  /**
   * @brief Print map sorted by values (ascending).
   * @param m   Map whose value type supports operator<.
   */
  template <typename Map>
    requires traits::is_map_like_v<Map>
  void print_sorted_by_value(const Map &m)
  {
    using K = typename Map::key_type;
    using V = typename Map::mapped_type;
    std::vector<std::pair<K, V>> items(m.begin(), m.end());
    std::sort(items.begin(), items.end(),
              [](const auto &a, const auto &b)
              { return a.second < b.second; });
    detail::write_range(*default_config().out, items, '{', '}',
                        default_config().max_items, false);
    *default_config().out << '\n';
  }

  /**
   * @brief Print only elements of a range satisfying a predicate.
   * @param rng   Source range.
   * @param pred  Predicate (element -> bool).
   */
  template <typename Range, typename Pred>
    requires traits::is_range_v<Range>
  void print_where(const Range &rng, Pred &&pred)
  {
    auto &os = *default_config().out;
    os << '[';
    bool first = true;
    std::size_t count = 0;
    for (const auto &elem : rng)
    {
      if (count >= default_config().max_items)
      {
        os << ", ...";
        break;
      }
      if (pred(elem))
      {
        if (!first)
          os << ", ";
        first = false;
        detail::write(os, elem);
        ++count;
      }
    }
    os << "]\n";
  }

  /**
   * @brief Print a range transformed by a function.
   * @param rng   Source range.
   * @param fn    Transform function (element -> U).
   */
  template <typename Range, typename Fn>
    requires traits::is_range_v<Range>
  void print_map_fn(const Range &rng, Fn &&fn)
  {
    auto &os = *default_config().out;
    os << '[';
    bool first = true;
    std::size_t count = 0;
    for (const auto &elem : rng)
    {
      if (count++ >= default_config().max_items)
      {
        os << ", ...";
        break;
      }
      if (!first)
        os << ", ";
      first = false;
      detail::write(os, fn(elem));
    }
    os << "]\n";
  }

  /**
   * @brief A lightweight string builder that appends vix-formatted tokens.
   *
   * @example
   *   vix::string_builder sb;
   *   sb.append("Result: ").append(42).append(", values: ").append(vec);
   *   std::string s = sb.str();
   */
  class string_builder
  {
  public:
    string_builder() = default;

    template <typename T>
    string_builder &append(const T &v)
    {
      detail::write(buf_, v);
      return *this;
    }

    string_builder &append_raw(std::string_view sv)
    {
      buf_ << sv;
      return *this;
    }

    string_builder &sep(std::string_view s = " ")
    {
      if (has_content_)
        buf_ << s;
      has_content_ = true;
      return *this;
    }

    template <typename T>
    string_builder &sep_append(const T &v, std::string_view s = " ")
    {
      return sep(s).append(v);
    }

    [[nodiscard]] std::string str() const { return buf_.str(); }
    [[nodiscard]] bool empty() const { return buf_.str().empty(); }

    void clear()
    {
      buf_.str({});
      buf_.clear();
      has_content_ = false;
    }

    /// Implicit conversion to std::string
    explicit operator std::string() const { return str(); }

    friend std::ostream &operator<<(std::ostream &os, const string_builder &sb)
    {
      return os << sb.str();
    }

  private:
    std::ostringstream buf_;
    bool has_content_ = false;
  };

  /**
   * @brief Print only if a condition is true.
   * @param cond   Condition.
   * @param args   Values to print if condition holds.
   */
  template <typename... Args>
  void print_if(bool cond, const Args &...args)
  {
    if (cond)
      detail::print_impl(default_config(), args...);
  }

  /**
   * @brief Print only in debug builds (when NDEBUG is not defined).
   */
  template <typename... Args>
  void dprint([[maybe_unused]] const Args &...args)
  {
#ifndef NDEBUG
    print_config cfg = default_config();
    cfg.out = &std::cerr;
    detail::print_impl(cfg, args...);
#endif
  }

  /**
   * @brief Print value and return it unchanged (inspect without disrupting flow).
   *
   * @example
   *   int result = vix::tap(compute_something());
   *   // Prints the result and returns it for further use.
   */
  template <typename T>
  const T &tap(const T &value, std::string_view label = "")
  {
    auto &os = *default_config().out;
    if (!label.empty())
      os << label << ": ";
    detail::write(os, value);
    os << '\n';
    return value;
  }

  /**
   * @brief Print a value with its type name.
   *
   * @example
   *   vix::print_typed(42);  // prints: int: 42
   */
  template <typename T>
  void print_typed(const T &v)
  {
    auto &os = *default_config().out;
    os << typeid(std::remove_cvref_t<T>).name() << ": ";
    detail::write(os, v);
    os << '\n';
  }

  /**
   * @brief Print two values side-by-side for comparison.
   */
  template <typename A, typename B>
  void print_diff(
      const A &a, const B &b,
      std::string_view label_a = "left",
      std::string_view label_b = "right")
  {
    auto &os = *default_config().out;
    os << label_a << ": ";
    detail::write(os, a);
    os << '\n';
    os << label_b << ": ";
    detail::write(os, b);
    os << '\n';
    os << "equal: " << std::boolalpha;
    if constexpr (requires { a == b; })
    {
      os << (a == b);
    }
    else
    {
      os << "<non-comparable>";
    }
    os << '\n';
  }

  /**
   * @brief Assert-and-print: print a labelled check result.
   *
   * Does not throw — only prints [PASS] or [FAIL] and the values.
   */
  template <typename A, typename B>
  bool print_check(std::string_view label, const A &a, const B &b)
  {
    auto &os = *default_config().out;
    bool eq = false;
    if constexpr (requires { a == b; })
    {
      eq = (a == b);
    }
    os << (eq ? "[PASS] " : "[FAIL] ") << label << '\n';
    if (!eq)
    {
      os << "  expected: ";
      detail::write(os, b);
      os << '\n';
      os << "  got:      ";
      detail::write(os, a);
      os << '\n';
    }
    return eq;
  }

  namespace ansi
  {
    // ANSI escape sequences
    inline constexpr std::string_view RESET = "\033[0m";
    inline constexpr std::string_view BOLD = "\033[1m";
    inline constexpr std::string_view RED = "\033[31m";
    inline constexpr std::string_view GREEN = "\033[32m";
    inline constexpr std::string_view YELLOW = "\033[33m";
    inline constexpr std::string_view BLUE = "\033[34m";
    inline constexpr std::string_view MAGENTA = "\033[35m";
    inline constexpr std::string_view CYAN = "\033[36m";
    inline constexpr std::string_view WHITE = "\033[37m";
    inline constexpr std::string_view GRAY = "\033[90m";

    /**
     * @brief Wrap a string in ANSI color codes.
     * @param s       Content.
     * @param color   ANSI escape sequence.
     * @return        Colored string.
     */
    [[nodiscard]] inline std::string colorize(
        std::string_view s,
        std::string_view color)
    {
      return std::string{color} + std::string{s} + std::string{RESET};
    }

    /**
     * @brief Print a success message in green.
     */
    template <typename... Args>
    void print_ok(const Args &...args)
    {
      *default_config().out << GREEN;
      detail::print_impl(default_config(), args...);
      *default_config().out << RESET;
    }

    /**
     * @brief Print an error message in red.
     */
    template <typename... Args>
    void print_err(const Args &...args)
    {
      print_config cfg = default_config();
      cfg.out = &std::cerr;
      *cfg.out << RED;
      detail::print_impl(cfg, args...);
      *cfg.out << RESET;
    }

    /**
     * @brief Print a warning message in yellow.
     */
    template <typename... Args>
    void print_warn(const Args &...args)
    {
      *default_config().out << YELLOW;
      detail::print_impl(default_config(), args...);
      *default_config().out << RESET;
    }

  } // namespace ansi

#ifdef VIX_ENABLE_MACROS

/// Print with file/line info.
#define VIX_LOG(...)         \
  ::vix::print_to(std::cerr, \
                  "[", __FILE__, ":", __LINE__, "] ", __VA_ARGS__)

/// Debug-only print with file/line.
#define VIX_DLOG(...) \
  ::vix::dprint("[", __FILE__, ":", __LINE__, "] ", __VA_ARGS__)

/// Inspect a variable: prints "varname = value"
#define VIX_INSPECT(x) \
  ::vix::print_named(#x, (x))

/// Assert with print on failure (no abort).
#define VIX_CHECK(a, b) \
  ::vix::print_check(#a " == " #b, (a), (b))

#endif // VIX_ENABLE_MACROS

  /**
   * @brief Summarize a range: show size, first N elements, and last M elements.
   *
   * @param rng   The range to summarize.
   * @param head  How many elements to show from the start.
   * @param tail  How many elements to show from the end.
   */
  template <typename Range>
    requires traits::is_range_v<Range>
  void print_summary(
      const Range &rng,
      std::size_t head = 3,
      std::size_t tail = 3)
  {
    auto &os = *default_config().out;

    // Collect into vector for random access
    using V = std::remove_cvref_t<decltype(*std::begin(rng))>;
    std::vector<V> items(std::begin(rng), std::end(rng));
    const std::size_t n = items.size();

    os << "range[n=" << n << "](";

    if (n == 0)
    {
      os << ")\n";
      return;
    }

    std::size_t show_head = std::min(head, n);
    for (std::size_t i = 0; i < show_head; ++i)
    {
      if (i > 0)
        os << ", ";
      detail::write(os, items[i]);
    }

    if (show_head < n)
    {
      std::size_t show_tail = std::min(tail, n - show_head);
      if (show_head + show_tail < n)
      {
        os << ", ...[" << (n - show_head - show_tail) << " hidden]..., ";
      }
      else
      {
        os << ", ";
      }
      std::size_t start = n - show_tail;
      for (std::size_t i = start; i < n; ++i)
      {
        if (i > start)
          os << ", ";
        detail::write(os, items[i]);
      }
    }

    os << ")\n";
  }

  inline void print_boxed(
      std::string_view message,
      std::size_t min_width = 40)
  {
    auto &os = *default_config().out;
    std::size_t content_width = std::max(message.size() + 4, min_width);

    // Top border
    os << "\u250c";
    for (std::size_t i = 0; i < content_width; ++i)
      os << "\u2500";
    os << "\u2510\n";

    // Content line
    os << "\u2502  " << message;
    std::size_t pad = content_width - message.size() - 2;
    for (std::size_t i = 0; i < pad; ++i)
      os << ' ';
    os << "\u2502\n";

    // Bottom border
    os << "\u2514";
    for (std::size_t i = 0; i < content_width; ++i)
      os << "\u2500";
    os << "\u2518\n";
  }

  /**
   * @brief Print a value as boxed, rendering it via vix engine.
   */
  template <typename T>
  void print_boxed_value(std::string_view label, const T &v)
  {
    print_boxed(vix::sprint(label, ": ", v));
  }

} // namespace vix

#endif
