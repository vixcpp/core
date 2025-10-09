#ifndef VIX_JSON_SIMPLE_HPP
#define VIX_JSON_SIMPLE_HPP

#include <string>
#include <variant>
#include <vector>
#include <memory>
#include <initializer_list>
#include <type_traits>

namespace Vix::json
{

    // FWD (permet d’avoir des pointeurs pour casser la récursivité)
    struct array_t;
    struct kvs;

    // -----------------------------
    // token: unité de valeur JSON
    // -----------------------------
    // - supporte scalaires
    // - supporte tableaux (array_t)
    // - supporte objets (kvs) imbriqués
    struct token
    {
        using value_t = std::variant<
            std::monostate, // null
            bool,
            long long, // int64
            double,    // floating
            std::string,
            std::shared_ptr<array_t>,
            std::shared_ptr<kvs>>;

        value_t v{std::monostate{}};

        token() = default;
        token(std::nullptr_t) : v(std::monostate{}) {}
        token(bool b) : v(b) {}
        token(int i) : v(static_cast<long long>(i)) {}
        token(long long i) : v(i) {}
        token(double d) : v(d) {}
        token(const char *s) : v(std::string(s)) {}
        token(std::string s) : v(std::move(s)) {}

        // constructeurs implicites “wrapper” pour objets/arrays
        token(const kvs &obj);     // défini après kvs
        token(const array_t &arr); // défini après array_t
    };

    // -----------------------------
    // Objet JSON: liste plate k,v
    // -----------------------------
    struct kvs
    {
        std::vector<token> flat;

        kvs() = default;
        kvs(std::initializer_list<token> list) : flat(list) {}

        // 🔹 nouvelles surcharges dynamiques
        explicit kvs(const std::vector<token> &v) : flat(v) {}
        explicit kvs(std::vector<token> &&v) : flat(std::move(v)) {}
    };

    // -----------------------------
    // Tableau JSON: liste de token
    // -----------------------------
    struct array_t
    {
        std::vector<token> elems;

        array_t() = default;
        array_t(std::initializer_list<token> l) : elems(l) {}

        // 🔹 nouvelles surcharges dynamiques
        explicit array_t(const std::vector<token> &v) : elems(v) {}
        explicit array_t(std::vector<token> &&v) : elems(std::move(v)) {}
    };

    // constructeurs dépendants désormais définis (après types complets)
    inline token::token(const kvs &obj)
        : v(std::make_shared<kvs>(obj)) {}

    inline token::token(const array_t &arr)
        : v(std::make_shared<array_t>(arr)) {}

    // -----------------------------
    // Helpers conviviaux
    // -----------------------------
    inline array_t array(std::initializer_list<token> l)
    {
        return array_t{l};
    }

    inline kvs obj(std::initializer_list<token> l)
    {
        return kvs{l};
    }

    // 🔹 nouvelles surcharges pour les helpers dynamiques
    inline array_t array(const std::vector<token> &v)
    {
        return array_t{v};
    }

    inline array_t array(std::vector<token> &&v)
    {
        return array_t{std::move(v)};
    }

    inline kvs obj(const std::vector<token> &v)
    {
        return kvs{v};
    }

    inline kvs obj(std::vector<token> &&v)
    {
        return kvs{std::move(v)};
    }

} // namespace Vix::json

#endif // VIX_JSON_SIMPLE_HPP
