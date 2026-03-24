/**
 * @file ResponseWrapper.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 */

#ifndef VIX_RESPONSE_WRAPPER_HPP
#define VIX_RESPONSE_WRAPPER_HPP

#include <cctype>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <memory>

#include <nlohmann/json.hpp>

#include <vix/http/Response.hpp>
#include <vix/http/Status.hpp>
#include <vix/json/Simple.hpp>

namespace vix::vhttp
{
  using OrderedJson = nlohmann::ordered_json;

  inline nlohmann::json token_to_nlohmann(const vix::json::token &t);
  inline nlohmann::json kvs_to_nlohmann(const vix::json::kvs &list);

  /** @brief Write an ordered JSON response into a native Vix response with the given status. */
  inline void ordered_json_response(
      Response &res,
      const OrderedJson &j,
      int status_code = OK)
  {
    res.set_status(normalize_status(status_code));
    res.set_header("Content-Type", "application/json; charset=utf-8");
    res.set_body(j.dump());
  }

  /** @brief Convert a vix::json token into nlohmann::json. */
  inline nlohmann::json token_to_nlohmann(const vix::json::token &t)
  {
    nlohmann::json j = nullptr;

    std::visit(
        [&](auto &&val)
        {
          using T = std::decay_t<decltype(val)>;

          if constexpr (std::is_same_v<T, std::monostate>)
          {
            j = nullptr;
          }
          else if constexpr (std::is_same_v<T, bool> ||
                             std::is_same_v<T, long long> ||
                             std::is_same_v<T, double> ||
                             std::is_same_v<T, std::string>)
          {
            j = val;
          }
          else if constexpr (std::is_same_v<T, std::shared_ptr<vix::json::array_t>>)
          {
            if (!val)
            {
              j = nullptr;
              return;
            }

            j = nlohmann::json::array();
            for (const auto &el : val->elems)
            {
              j.push_back(token_to_nlohmann(el));
            }
          }
          else if constexpr (std::is_same_v<T, std::shared_ptr<vix::json::kvs>>)
          {
            if (!val)
            {
              j = nullptr;
              return;
            }

            j = kvs_to_nlohmann(*val);
          }
          else
          {
            j = nullptr;
          }
        },
        t.v);

    return j;
  }

  /** @brief Convert a vix::json key-value list into an object-like nlohmann::json value. */
  inline nlohmann::json kvs_to_nlohmann(const vix::json::kvs &list)
  {
    nlohmann::json obj = nlohmann::json::object();
    const auto &a = list.flat;
    const size_t n = a.size() - (a.size() % 2);

    for (size_t i = 0; i < n; i += 2)
    {
      const auto &k = a[i].v;
      const auto &v = a[i + 1];

      if (!std::holds_alternative<std::string>(k))
        continue;

      const std::string &key = std::get<std::string>(k);
      obj[key] = token_to_nlohmann(v);
    }

    return obj;
  }

  /** @brief Lightweight response helper that sets status/headers and sends text, JSON, redirects, or static files. */
  struct ResponseWrapper
  {
    Response &res;

    /** @brief Wrap an existing native Vix response and ensure a default status code. */
    explicit ResponseWrapper(Response &r) noexcept
        : res(r)
    {
      if (!is_valid_status(res.status()))
        res.set_status(OK);
    }

    /** @brief Return a best-effort MIME type for a file extension (including leading dot). */
    static inline std::string mime_from_ext(std::string_view ext)
    {
      static const std::unordered_map<std::string, std::string> m{
          {".html", "text/html; charset=utf-8"},
          {".css", "text/css; charset=utf-8"},
          {".js", "application/javascript; charset=utf-8"},
          {".json", "application/json; charset=utf-8"},
          {".png", "image/png"},
          {".jpg", "image/jpeg"},
          {".jpeg", "image/jpeg"},
          {".gif", "image/gif"},
          {".svg", "image/svg+xml"},
          {".ico", "image/x-icon"},
          {".txt", "text/plain; charset=utf-8"},
          {".woff", "font/woff"},
          {".woff2", "font/woff2"}};

      auto it = m.find(std::string(ext));
      return it == m.end() ? "application/octet-stream" : it->second;
    }

    /** @brief Read an entire file as binary into a string and return false on error. */
    static inline bool read_file_binary(const std::filesystem::path &p, std::string &out)
    {
      std::ifstream f(p, std::ios::binary);
      if (!f)
        return false;

      f.seekg(0, std::ios::end);
      const std::streamsize n = f.tellg();
      if (n < 0)
        return false;
      f.seekg(0, std::ios::beg);

      out.resize(static_cast<std::size_t>(n));
      if (n > 0)
        f.read(out.data(), n);

      return f.good() || f.eof();
    }

    /** @brief Send a static file (auto index.html for directories) with basic path safety and MIME detection. */
    ResponseWrapper &file(std::filesystem::path p)
    {
      ensure_status();

      const std::string s = p.lexically_normal().generic_string();
      if (s.find("..") != std::string::npos)
        return status(BAD_REQUEST).text("Bad path");

      std::error_code ec;
      if (std::filesystem::is_directory(p, ec))
      {
        p /= "index.html";
      }

      if (!std::filesystem::exists(p, ec) || !std::filesystem::is_regular_file(p, ec))
        return status(NOT_FOUND).text("Not Found");

      std::string body;
      if (!read_file_binary(p, body))
        return status(INTERNAL_ERROR).text("File read error");

      std::string ext = p.extension().string();
      if (!ext.empty())
      {
        for (char &c : ext)
        {
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
      }

      std::string mime = "application/octet-stream";
      if (!ext.empty())
      {
        mime = mime_from_ext(ext);
      }
      else
      {
        if (body.rfind("<!doctype html", 0) == 0 || body.rfind("<html", 0) == 0)
          mime = "text/html; charset=utf-8";
      }

      type(mime);
      header("X-Content-Type-Options", "nosniff");

      if (!has_header("Cache-Control"))
        header("Cache-Control", "public, max-age=3600");

      res.set_body(std::move(body));
      return *this;
    }

    /** @brief Ensure the response has a valid status (defaults to 200 OK). */
    void ensure_status() noexcept
    {
      if (!is_valid_status(res.status()))
        res.set_status(OK);
    }

    /** @brief Return true if the response already contains the given header. */
    bool has_header(std::string_view key) const
    {
      return res.has_header(key);
    }

    /** @brief Return true if the response body is non-empty. */
    bool has_body() const
    {
      return !res.body().empty();
    }

    /** @brief Return a default message for a numeric status code (e.g. 404 -> "404 Not Found"). */
    static std::string default_status_message(int code)
    {
      return vix::vhttp::status_to_string(code);
    }

    /** @brief Set the HTTP status code using an integer in [100..599]. */
    ResponseWrapper &status(int code)
    {
#ifndef NDEBUG
      if (!is_valid_status(code))
      {
        throw std::runtime_error(
            "Invalid HTTP status code: " + std::to_string(code) +
            ". Status code must be between 100 and 599.");
      }
#endif
      res.set_status(normalize_status(code));
      return *this;
    }

    /** @brief Alias for status(int). */
    ResponseWrapper &set_status(int code)
    {
      return status(code);
    }

    /** @brief Set a compile-time status code constant. */
    template <int Code>
    ResponseWrapper &status_c() noexcept
    {
      static_assert(Code >= 100 && Code <= 599, "HTTP status code must be in [100..599]");
      res.set_status(Code);
      return *this;
    }

    /** @brief Alias for status_c<Code>(). */
    template <int Code>
    ResponseWrapper &set_status_c() noexcept
    {
      return status_c<Code>();
    }

    /** @brief Set or replace a header. */
    ResponseWrapper &header(std::string_view key, std::string_view value)
    {
      res.set_header(std::string(key), std::string(value));
      return *this;
    }

    /** @brief Alias for header(key, value). */
    ResponseWrapper &set(std::string_view key, std::string_view value)
    {
      return header(key, value);
    }

    /** @brief Append a value to a header as a comma-separated list. */
    ResponseWrapper &append(std::string_view key, std::string_view value)
    {
      std::string existing = res.header(key);
      if (existing.empty())
      {
        return header(key, value);
      }

      existing += ", ";
      existing += value;
      res.set_header(std::string(key), std::move(existing));
      return *this;
    }

    /** @brief Set the Content-Type header. */
    ResponseWrapper &type(std::string_view mime)
    {
      res.set_header("Content-Type", std::string(mime));
      return *this;
    }

    /** @brief Alias for type(mime). */
    ResponseWrapper &contentType(std::string_view mime)
    {
      return type(mime);
    }

    /** @brief Send a 302 redirect to the given URL. */
    ResponseWrapper &redirect(std::string_view url)
    {
      return redirect(FOUND, url);
    }

    /** @brief Send a redirect response with a specific status code (e.g. 301, 302, 307, 308). */
    ResponseWrapper &redirect(int code, std::string_view url)
    {
      status(code);
      header("Location", url);

      if (!has_header("Content-Type"))
      {
        type("text/html; charset=utf-8");
        header("X-Content-Type-Options", "nosniff");
      }

      std::string body;
      body.reserve(256);
      body += "<!doctype html><html><head><meta charset=\"utf-8\"></head><body>";
      body += "Redirecting to ";
      body += std::string(url);
      body += "</body></html>";

      Response::text_response(res, body, res.status());
      return *this;
    }

    /** @brief Send an empty response for no-content statuses or a default body for other statuses. */
    ResponseWrapper &sendStatus(int code)
    {
      status(code);

      const int s = res.status();
      if (s == NO_CONTENT || s == NOT_MODIFIED)
        return this->send();

      return send(default_status_message(s));
    }

    /** @brief Send plain text with an auto Content-Type if missing. */
    ResponseWrapper &text(std::string_view data)
    {
      ensure_status();

      const int s = res.status();
      if (s == NO_CONTENT || s == NOT_MODIFIED)
        return this->send();

      if (!has_header("Content-Type"))
      {
        type("text/plain; charset=utf-8");
        header("X-Content-Type-Options", "nosniff");
      }

      Response::text_response(res, data, res.status());
      return *this;
    }

    /** @brief Send JSON using nlohmann::json with an auto Content-Type if missing. */
    ResponseWrapper &json(const nlohmann::json &j)
    {
      ensure_status();

      const int s = res.status();
      if (s == NO_CONTENT || s == NOT_MODIFIED)
        return this->send();

      if (!has_header("Content-Type"))
      {
        type("application/json; charset=utf-8");
        header("X-Content-Type-Options", "nosniff");
      }

      Response::json_response(res, j, res.status());
      return *this;
    }

    /** @brief Send JSON from a vix::json key-value list. */
    ResponseWrapper &json(const vix::json::kvs &kv)
    {
      auto j = kvs_to_nlohmann(kv);
      return json(j);
    }

    /** @brief Send JSON from an initializer list of vix::json tokens (key/value pairs). */
    ResponseWrapper &json(std::initializer_list<vix::json::token> list)
    {
      return json(vix::json::kvs{list});
    }

    /** @brief Send ordered JSON (stable key order) with an auto Content-Type if missing. */
    ResponseWrapper &json_ordered(const OrderedJson &j)
    {
      ensure_status();

      const int s = res.status();
      if (s == NO_CONTENT || s == NOT_MODIFIED)
        return this->send();

      if (!has_header("Content-Type"))
      {
        type("application/json; charset=utf-8");
        header("X-Content-Type-Options", "nosniff");
      }

      ordered_json_response(res, j, res.status());
      return *this;
    }

    /** @brief Send JSON from any serializable type supported by vix::vhttp::Response::json_response. */
    template <typename J>
      requires(!std::is_same_v<std::decay_t<J>, nlohmann::json> &&
               !std::is_same_v<std::decay_t<J>, vix::json::kvs> &&
               !std::is_same_v<std::decay_t<J>, std::initializer_list<vix::json::token>> &&
               !std::is_same_v<std::decay_t<J>, OrderedJson>)
    ResponseWrapper &json(const J &data)
    {
      ensure_status();

      const int s = res.status();
      if (s == NO_CONTENT || s == NOT_MODIFIED)
        return this->send();

      if (!has_header("Content-Type"))
      {
        type("application/json; charset=utf-8");
        header("X-Content-Type-Options", "nosniff");
      }

      Response::json_response(res, data, res.status());
      return *this;
    }

    /** @brief Finalize the response by ensuring a body when appropriate. */
    ResponseWrapper &send()
    {
      ensure_status();

      const int s = res.status();
      if (s == NO_CONTENT || s == NOT_MODIFIED)
      {
        res.set_body("");
        return *this;
      }

      if (!has_body())
      {
        return text(default_status_message(s));
      }

      return *this;
    }

    /** @brief Alias for send(). */
    ResponseWrapper &end()
    {
      return send();
    }

    /** @brief Set the Location header (use with status(3xx) for redirects). */
    ResponseWrapper &location(std::string_view url)
    {
      return header("Location", url);
    }

    /** @brief Send plain text (alias for text()). */
    ResponseWrapper &send(std::string_view data)
    {
      return text(data);
    }

    /** @brief Send plain text (null-safe). */
    ResponseWrapper &send(const char *data)
    {
      return text(data ? std::string_view{data} : std::string_view{});
    }

    /** @brief Send plain text from std::string. */
    ResponseWrapper &send(const std::string &data)
    {
      return text(std::string_view{data});
    }

    /** @brief Send JSON (alias for json()). */
    ResponseWrapper &send(const nlohmann::json &j)
    {
      return json(j);
    }

    /** @brief Send JSON from vix::json key-value list. */
    ResponseWrapper &send(const vix::json::kvs &kv)
    {
      return json(kv);
    }

    /** @brief Send JSON from initializer list of vix::json tokens. */
    ResponseWrapper &send(std::initializer_list<vix::json::token> list)
    {
      return json(list);
    }

    /** @brief Send ordered JSON (stable key order). */
    ResponseWrapper &send(const OrderedJson &j)
    {
      return json_ordered(j);
    }

    /** @brief Send JSON from a custom type (must be supported by vix::vhttp::Response::json_response). */
    template <typename J>
      requires(!std::is_same_v<std::decay_t<J>, nlohmann::json> &&
               !std::is_same_v<std::decay_t<J>, vix::json::kvs> &&
               !std::is_same_v<std::decay_t<J>, std::initializer_list<vix::json::token>> &&
               !std::is_same_v<std::decay_t<J>, OrderedJson> &&
               !std::is_convertible_v<J, std::string_view>)
    ResponseWrapper &send(const J &data)
    {
      return json(data);
    }

    /** @brief Set status then send a payload (text or JSON) in one call. */
    template <typename T>
    ResponseWrapper &send(int statusCode, const T &payload)
    {
      status(statusCode);
      return send(payload);
    }

    /** @brief Set a compile-time status code then send a payload (text or JSON). */
    template <int Code, typename T>
    ResponseWrapper &send_c(const T &payload)
    {
      status_c<Code>();
      return send(payload);
    }

    /** @brief Convenience: set status to 200 OK. */
    ResponseWrapper &ok() { return status(OK); }

    /** @brief Convenience: set status to 201 Created. */
    ResponseWrapper &created() { return status(CREATED); }

    /** @brief Convenience: set status to 202 Accepted. */
    ResponseWrapper &accepted() { return status(ACCEPTED); }

    /** @brief Convenience: set status to 204 No Content. */
    ResponseWrapper &no_content() { return status(NO_CONTENT); }

    /** @brief Convenience: set status to 400 Bad Request. */
    ResponseWrapper &bad_request() { return status(BAD_REQUEST); }

    /** @brief Convenience: set status to 401 Unauthorized. */
    ResponseWrapper &unauthorized() { return status(UNAUTHORIZED); }

    /** @brief Convenience: set status to 403 Forbidden. */
    ResponseWrapper &forbidden() { return status(FORBIDDEN); }

    /** @brief Convenience: set status to 404 Not Found. */
    ResponseWrapper &not_found() { return status(NOT_FOUND); }

    /** @brief Convenience: set status to 409 Conflict. */
    ResponseWrapper &conflict() { return status(CONFLICT); }

    /** @brief Convenience: set status to 500 Internal Server Error. */
    ResponseWrapper &internal_error() { return status(INTERNAL_ERROR); }

    /** @brief Convenience: set status to 501 Not Implemented. */
    ResponseWrapper &not_implemented() { return status(NOT_IMPLEMENTED); }

    /** @brief Convenience: set status to 502 Bad Gateway. */
    ResponseWrapper &bad_gateway() { return status(BAD_GATEWAY); }

    /** @brief Convenience: set status to 503 Service Unavailable. */
    ResponseWrapper &service_unavailable() { return status(SERVICE_UNAVAILABLE); }
  };

} // namespace vix::vhttp

#endif // VIX_RESPONSE_WRAPPER_HPP
