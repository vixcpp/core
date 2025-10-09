#ifndef VIX_JSON_SIMPLE_HPP
#define VIX_JSON_SIMPLE_HPP

/**
 * @file Simple.hpp
 * @brief Minimal JSON-like data model for lightweight Vix internal APIs.
 *
 * @details
 * `Vix::json::Simple` provides a self-contained JSON representation for internal
 * use, independent from `nlohmann::json`. It is designed to be header-only and
 * trivially embeddable in performance-sensitive modules or plugins.
 *
 * Features:
 * - `token`: a tagged variant supporting scalars, arrays, and objects.
 * - `array_t`: a flat sequence of tokens.
 * - `kvs`: a flattened key/value list representing JSON objects.
 * - Helper functions (`obj()` and `array()`) for quick construction.
 * - Implicit constructors for nesting (tokens can wrap `array_t` or `kvs`).
 *
 * Example:
 * ```cpp
 * using namespace Vix::json;
 *
 * kvs user = obj({
 *     "name", "Alice",
 *     "age", 30,
 *     "skills", array({"C++", "Networking", "Systems"})
 * });
 *
 * token t = user; // convertible to token
 * ```
 *
 * ### Design notes
 * - This type avoids dynamic JSON parsing/serialization overhead when
 *   interoperating between different internal JSON adapters.
 * - Recursive types are handled using shared_ptr wrappers.
 */

#include <string>
#include <variant>
#include <vector>
#include <memory>
#include <initializer_list>
#include <type_traits>

namespace Vix::json
{

    struct array_t;
    struct kvs;

    // ------------------------------------------------------------------
    // token — base value unit for JSON
    // ------------------------------------------------------------------
    struct token
    {
        using value_t = std::variant<
            std::monostate,           // null
            bool,                     // boolean
            long long,                // integer (64-bit)
            double,                   // floating point
            std::string,              // string
            std::shared_ptr<array_t>, // array
            std::shared_ptr<kvs>      // object
            >;

        value_t v{std::monostate{}};

        // Constructors for scalar values
        token() = default;
        token(std::nullptr_t) : v(std::monostate{}) {}
        token(bool b) : v(b) {}
        token(int i) : v(static_cast<long long>(i)) {}
        token(long long i) : v(i) {}
        token(double d) : v(d) {}
        token(const char *s) : v(std::string(s)) {}
        token(std::string s) : v(std::move(s)) {}

        // Implicit wrappers for nested object/array types
        token(const kvs &obj);
        token(const array_t &arr);
    };

    // ------------------------------------------------------------------
    // kvs — lightweight JSON object representation (flat key/value list)
    // ------------------------------------------------------------------
    struct kvs
    {
        std::vector<token> flat;

        kvs() = default;
        kvs(std::initializer_list<token> list) : flat(list) {}
        explicit kvs(const std::vector<token> &v) : flat(v) {}
        explicit kvs(std::vector<token> &&v) : flat(std::move(v)) {}
    };

    // ------------------------------------------------------------------
    // array_t — lightweight JSON array representation
    // ------------------------------------------------------------------
    struct array_t
    {
        std::vector<token> elems;

        array_t() = default;
        array_t(std::initializer_list<token> l) : elems(l) {}
        explicit array_t(const std::vector<token> &v) : elems(v) {}
        explicit array_t(std::vector<token> &&v) : elems(std::move(v)) {}
    };

    // Define token's object/array constructors after full types are known
    inline token::token(const kvs &obj) : v(std::make_shared<kvs>(obj)) {}
    inline token::token(const array_t &arr) : v(std::make_shared<array_t>(arr)) {}

    // ------------------------------------------------------------------
    // Helpers for quick construction
    // ------------------------------------------------------------------
    inline array_t array(std::initializer_list<token> l) { return array_t{l}; }
    inline kvs obj(std::initializer_list<token> l) { return kvs{l}; }

    // Overloads for dynamic vector-based usage
    inline array_t array(const std::vector<token> &v) { return array_t{v}; }
    inline array_t array(std::vector<token> &&v) { return array_t{std::move(v)}; }
    inline kvs obj(const std::vector<token> &v) { return kvs{v}; }
    inline kvs obj(std::vector<token> &&v) { return kvs{std::move(v)}; }

} // namespace Vix::json

#endif // VIX_JSON_SIMPLE_HPP