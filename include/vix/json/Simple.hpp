#ifndef VIX_SIMPLE_HPP
#define VIX_SIMPLE_HPP

#include <string>
#include <variant>
#include <vector>
#include <initializer_list>

namespace Vix::json
{
    using Value = std::variant<std::monostate, bool, int, long long, double, std::string>;

    struct token
    {
        Value v;
        token(const char *s) : v(std::string(s)) {}
        token(std::string s) : v(std::move(s)) {}
        token(bool b) : v(b) {}
        token(int i) : v(i) {}
        token(long long i) : v(i) {}
        token(double d) : v(d) {}
        token(std::nullptr_t) : v(std::monostate{}) {}
    };

    struct kvs
    {
        std::vector<token> flat;
        kvs(std::initializer_list<token> list) : flat(list) {}
    };
} // namespace Vix::json

#endif