/**
 *
 *  @file traits.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira. All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 *  Shared compile-time traits used across Vix modules such as print,
 *  format, and inspect.
 *
 */

#ifndef VIX_META_TRAITS_HPP
#define VIX_META_TRAITS_HPP

#include <any>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <stack>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>

#if defined(__cpp_lib_expected) && __cpp_lib_expected >= 202202L
#include <expected>
#define VIX_HAS_EXPECTED 1
#else
#define VIX_HAS_EXPECTED 0
#endif

namespace vix
{
  // Forward declarations for extension-point detection.
  struct inspect_context;

  template <typename T, typename = void>
  struct formatter;

  template <typename T, typename = void>
  struct inspector;

  template <typename T, typename = void>
  struct field_map;

  namespace traits
  {
    // ------------------------------------------------------------
    // ostreamability
    // ------------------------------------------------------------
    template <typename T, typename = void>
    struct is_ostreamable : std::false_type
    {
    };

    template <typename T>
    struct is_ostreamable<
        T,
        std::void_t<decltype(std::declval<std::ostream &>() << std::declval<const T &>())>>
        : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_ostreamable_v = is_ostreamable<T>::value;

    // ------------------------------------------------------------
    // string-like
    // ------------------------------------------------------------
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

    template <>
    struct is_string_like<wchar_t *> : std::true_type
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

    // ------------------------------------------------------------
    // range
    // ------------------------------------------------------------
    template <typename T, typename = void>
    struct is_range : std::false_type
    {
    };

    template <typename T>
    struct is_range<
        T,
        std::void_t<
            decltype(std::begin(std::declval<const T &>())),
            decltype(std::end(std::declval<const T &>()))>>
        : std::bool_constant<!is_string_like_v<T>>
    {
    };

    template <typename T>
    inline constexpr bool is_range_v = is_range<T>::value;

    // ------------------------------------------------------------
    // map-like
    // ------------------------------------------------------------
    template <typename T, typename = void>
    struct is_map_like : std::false_type
    {
    };

    template <typename T>
    struct is_map_like<T, std::void_t<typename T::key_type, typename T::mapped_type>>
        : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_map_like_v = is_map_like<T>::value;

    // ------------------------------------------------------------
    // set-like
    // ------------------------------------------------------------
    template <typename T, typename = void>
    struct is_set_like : std::false_type
    {
    };

    template <typename T>
    struct is_set_like<T, std::void_t<typename T::key_type>>
        : std::bool_constant<!is_map_like_v<T>>
    {
    };

    template <typename T>
    inline constexpr bool is_set_like_v = is_set_like<T>::value;

    // ------------------------------------------------------------
    // tuple-like
    // ------------------------------------------------------------
    template <typename T, typename = void>
    struct is_tuple_like : std::false_type
    {
    };

    template <typename T>
    struct is_tuple_like<T, std::void_t<decltype(std::tuple_size<T>::value)>>
        : std::true_type
    {
    };

    template <>
    struct is_tuple_like<std::string> : std::false_type
    {
    };

    template <>
    struct is_tuple_like<std::wstring> : std::false_type
    {
    };

    template <typename T>
    inline constexpr bool is_tuple_like_v = is_tuple_like<std::remove_cvref_t<T>>::value;

    // ------------------------------------------------------------
    // pair
    // ------------------------------------------------------------
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

    // ------------------------------------------------------------
    // smart pointers
    // ------------------------------------------------------------
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

    // ------------------------------------------------------------
    // optional / variant / any
    // ------------------------------------------------------------
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

    template <typename T>
    inline constexpr bool is_any_v =
        std::is_same_v<std::remove_cvref_t<T>, std::any>;

    // ------------------------------------------------------------
    // chrono
    // ------------------------------------------------------------
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

    // ------------------------------------------------------------
    // filesystem::path
    // ------------------------------------------------------------
    template <typename T>
    inline constexpr bool is_fs_path_v =
        std::is_same_v<std::remove_cvref_t<T>, std::filesystem::path>;

    // ------------------------------------------------------------
    // reference_wrapper
    // ------------------------------------------------------------
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

    // alias kept for compatibility with existing inspect.hpp code
    template <typename T>
    using is_ref_wrapper = is_reference_wrapper<T>;

    template <typename T>
    inline constexpr bool is_ref_wrapper_v = is_reference_wrapper_v<T>;

    // ------------------------------------------------------------
    // raw pointer / function pointer / nullptr
    // ------------------------------------------------------------
    template <typename T>
    inline constexpr bool is_raw_pointer_v =
        std::is_pointer_v<T> &&
        !is_string_like_v<T> &&
        !std::is_function_v<std::remove_pointer_t<T>>;

    template <typename T>
    inline constexpr bool is_function_ptr_v =
        std::is_pointer_v<T> &&
        std::is_function_v<std::remove_pointer_t<T>>;

    template <typename T>
    inline constexpr bool is_nullptr_v =
        std::is_same_v<std::remove_cvref_t<T>, std::nullptr_t>;

    // ------------------------------------------------------------
    // bool / char / enum
    // ------------------------------------------------------------
    template <typename T>
    inline constexpr bool is_bool_v =
        std::is_same_v<std::remove_cvref_t<T>, bool>;

    template <typename T>
    inline constexpr bool is_char_v =
        std::is_same_v<std::remove_cvref_t<T>, char> ||
        std::is_same_v<std::remove_cvref_t<T>, signed char> ||
        std::is_same_v<std::remove_cvref_t<T>, unsigned char> ||
        std::is_same_v<std::remove_cvref_t<T>, wchar_t> ||
        std::is_same_v<std::remove_cvref_t<T>, char8_t> ||
        std::is_same_v<std::remove_cvref_t<T>, char16_t> ||
        std::is_same_v<std::remove_cvref_t<T>, char32_t>;

    template <typename T>
    inline constexpr bool is_enum_v =
        std::is_enum_v<std::remove_cvref_t<T>>;

    // ------------------------------------------------------------
    // container adapters
    // ------------------------------------------------------------
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

    // ------------------------------------------------------------
    // formatter detection
    // ------------------------------------------------------------
    template <typename T, typename = void>
    struct has_formatter_specialization : std::false_type
    {
    };

    template <typename T>
    struct has_formatter_specialization<
        T,
        std::void_t<decltype(formatter<T>::format(
            std::declval<std::ostream &>(),
            std::declval<const T &>()))>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool has_formatter_v =
        has_formatter_specialization<std::remove_cvref_t<T>>::value;

    // ------------------------------------------------------------
    // ADL vix_format detection
    // ------------------------------------------------------------
    template <typename T, typename = void>
    struct has_vix_format : std::false_type
    {
    };

    template <typename T>
    struct has_vix_format<
        T,
        std::void_t<decltype(vix_format(
            std::declval<std::ostream &>(),
            std::declval<const T &>()))>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool has_vix_format_v = has_vix_format<T>::value;

    // ------------------------------------------------------------
    // inspect-specific extension points
    // ------------------------------------------------------------
    template <typename T, typename = void>
    struct has_vix_inspect_hook : std::false_type
    {
    };

    template <typename T>
    struct has_vix_inspect_hook<
        T,
        std::void_t<decltype(vix_inspect(
            std::declval<inspect_context &>(),
            std::declval<const T &>()))>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool has_vix_inspect_hook_v =
        has_vix_inspect_hook<std::remove_cvref_t<T>>::value;

    template <typename T, typename = void>
    struct has_inspector_specialization : std::false_type
    {
    };

    template <typename T>
    struct has_inspector_specialization<
        T,
        std::void_t<decltype(inspector<T>::inspect(
            std::declval<inspect_context &>(),
            std::declval<const T &>()))>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool has_inspector_v =
        has_inspector_specialization<std::remove_cvref_t<T>>::value;

    template <typename T, typename = void>
    struct has_field_map : std::false_type
    {
    };

    template <typename T>
    struct has_field_map<T, std::void_t<decltype(field_map<T>::fields())>>
        : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool has_field_map_v =
        has_field_map<std::remove_cvref_t<T>>::value;

#if VIX_HAS_EXPECTED
    // ------------------------------------------------------------
    // std::expected
    // ------------------------------------------------------------
    template <typename T>
    struct is_expected : std::false_type
    {
    };

    template <typename T, typename E>
    struct is_expected<std::expected<T, E>> : std::true_type
    {
    };

    template <typename T>
    inline constexpr bool is_expected_v =
        is_expected<std::remove_cvref_t<T>>::value;
#endif

  } // namespace traits

  namespace concepts
  {
    template <typename T>
    concept Streamable = traits::is_ostreamable_v<T>;

    template <typename T>
    concept Ostreamable = traits::is_ostreamable_v<T>;

    template <typename T>
    concept StringLike = traits::is_string_like_v<T>;

    template <typename T>
    concept Range = traits::is_range_v<T>;

    template <typename T>
    concept MapLike = traits::is_map_like_v<T>;

    template <typename T>
    concept SetLike = traits::is_set_like_v<T> && !MapLike<T>;

    template <typename T>
    concept TupleLike = traits::is_tuple_like_v<T> && !Range<T>;

    template <typename T>
    concept HasVixFormat = traits::has_vix_format_v<T>;

    template <typename T>
    concept HasVixInspect = traits::has_vix_inspect_hook_v<T>;

    template <typename T>
    concept HasInspector = traits::has_inspector_v<T>;

    template <typename T>
    concept HasFieldMap = traits::has_field_map_v<T>;

    template <typename T>
    concept SmartPointer = traits::is_smart_ptr_v<T>;

    template <typename T>
    concept Duration = traits::is_duration_v<T>;

    template <typename T>
    concept TimePoint = traits::is_time_point_v<T>;
  } // namespace concepts

} // namespace vix

#endif // VIX_META_TRAITS_HPP
