#pragma once

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vix::vhttp::cache
{
    class CacheKey
    {
    public:
        // method: "GET", path: "/users/1", query: "a=1&b=2" (optionnel)
        // headers: tous les headers reçus; include_headers: liste blanche pour varier la clé
        static std::string fromRequest(std::string_view method,
                                       std::string_view path,
                                       std::string_view query,
                                       const std::unordered_map<std::string, std::string> &headers,
                                       const std::vector<std::string> &include_headers = {})
        {
            std::string m = upper_(method);
            std::string p(path);
            std::string q = normalizeQuery_(query);

            std::string key;
            key.reserve(64 + p.size() + q.size());

            key += m;
            key += " ";
            key += p;

            if (!q.empty())
            {
                key += "?";
                key += q;
            }

            if (!include_headers.empty())
            {
                key += " |h:";
                for (const auto &name : include_headers)
                {
                    const std::string hn = lower_(name);
                    auto it = headers.find(name);
                    if (it == headers.end())
                    {
                        // tenter aussi lower-case lookup si ton code normalise ailleurs
                        auto it2 = headers.find(hn);
                        if (it2 != headers.end())
                        {
                            key += hn;
                            key += "=";
                            key += trim_(it2->second);
                            key += ";";
                        }
                        continue;
                    }

                    key += hn;
                    key += "=";
                    key += trim_(it->second);
                    key += ";";
                }
            }

            return key;
        }

    private:
        static std::string lower_(std::string_view s)
        {
            std::string out;
            out.reserve(s.size());
            for (char c : s)
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            return out;
        }

        static std::string upper_(std::string_view s)
        {
            std::string out;
            out.reserve(s.size());
            for (char c : s)
                out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            return out;
        }

        static std::string trim_(const std::string &v)
        {
            std::size_t b = 0;
            while (b < v.size() && std::isspace(static_cast<unsigned char>(v[b])))
                ++b;

            std::size_t e = v.size();
            while (e > b && std::isspace(static_cast<unsigned char>(v[e - 1])))
                --e;

            return v.substr(b, e - b);
        }

        static std::string normalizeQuery_(std::string_view query)
        {
            if (query.empty())
                return {};

            // parse "a=1&b=2" -> vector<pair<k,v>>, sort by k then v, rebuild
            std::vector<std::pair<std::string, std::string>> items;
            items.reserve(8);

            std::size_t i = 0;
            while (i < query.size())
            {
                std::size_t amp = query.find('&', i);
                if (amp == std::string_view::npos)
                    amp = query.size();

                std::string_view part = query.substr(i, amp - i);
                std::size_t eq = part.find('=');

                std::string k;
                std::string v;

                if (eq == std::string_view::npos)
                {
                    k = std::string(part);
                    v = "";
                }
                else
                {
                    k = std::string(part.substr(0, eq));
                    v = std::string(part.substr(eq + 1));
                }

                items.emplace_back(std::move(k), std::move(v));
                i = amp + 1;
            }

            std::sort(items.begin(), items.end(), [](const auto &a, const auto &b)
                      {
                if (a.first != b.first)
                    return a.first < b.first;
                return a.second < b.second; });

            std::ostringstream oss;
            for (std::size_t idx = 0; idx < items.size(); ++idx)
            {
                if (idx)
                    oss << "&";
                oss << items[idx].first;
                if (!items[idx].second.empty())
                    oss << "=" << items[idx].second;
            }
            return oss.str();
        }
    };

} // namespace vix::vhttp::cache
