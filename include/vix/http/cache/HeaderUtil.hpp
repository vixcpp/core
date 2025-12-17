#pragma once

#include <cctype>
#include <string>
#include <unordered_map>

namespace vix::http::cache
{
    struct HeaderUtil
    {
        static std::string toLower(const std::string &s)
        {
            std::string out;
            out.reserve(s.size());
            for (char c : s)
                out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            return out;
        }

        static void normalizeInPlace(std::unordered_map<std::string, std::string> &headers)
        {
            std::unordered_map<std::string, std::string> norm;
            norm.reserve(headers.size());

            for (const auto &kv : headers)
            {
                norm[toLower(kv.first)] = kv.second; // last-wins
            }
            headers.swap(norm);
        }
    };
} // namespace vix::http::cache
