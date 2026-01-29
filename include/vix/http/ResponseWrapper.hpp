/**
 *
 *  @file ResponseWrapper.hpp
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
#ifndef VIX_RESPONSE_WRAPPER_HPP
#define VIX_RESPONSE_WRAPPER_HPP

#include <string>
#include <stdexcept>
#include <initializer_list>
#include <filesystem>
#include <fstream>
#include <unordered_map>

#include <boost/beast/http.hpp>
#include <boost/beast/core/string.hpp>
#include <nlohmann/json.hpp>

#include <vix/http/Response.hpp>
#include <vix/http/Status.hpp>
#include <vix/json/Simple.hpp>

namespace vix::vhttp
{
  using OrderedJson = nlohmann::ordered_json;
  inline nlohmann::json token_to_nlohmann(const vix::json::token &t);
  inline nlohmann::json kvs_to_nlohmann(const vix::json::kvs &list);

  inline void ordered_json_response(
      http::response<http::string_body> &res,
      const OrderedJson &j,
      http::status status_code = http::status::ok)
  {
    res.result(status_code);
    res.body() = j.dump();
    res.set(http::field::content_type, "application/json");
    res.prepare_payload();
  }

  // JSON conversion utilities (vix::json → nlohmann::json)
  inline nlohmann::json token_to_nlohmann(const vix::json::token &t)
  {
    nlohmann::json j = nullptr;
    std::visit([&](auto &&val)
               {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                j = nullptr;
            } else if constexpr (std::is_same_v<T, bool> ||
                                 std::is_same_v<T, long long> ||
                                 std::is_same_v<T, double> ||
                                 std::is_same_v<T, std::string>) {
                j = val;
            } else if constexpr (std::is_same_v<T, std::shared_ptr<vix::json::array_t>>) {
                if (!val) { j = nullptr; return; }
                j = nlohmann::json::array();
                for (const auto& el : val->elems) {
                    j.push_back(token_to_nlohmann(el));
                }
            } else if constexpr (std::is_same_v<T, std::shared_ptr<vix::json::kvs>>) {
                if (!val) { j = nullptr; return; }
                j = kvs_to_nlohmann(*val);
            } else {
                j = nullptr;
            } }, t.v);
    return j;
  }

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

  struct ResponseWrapper
  {
    http::response<http::string_body> &res;

    explicit ResponseWrapper(http::response<http::string_body> &r) noexcept : res(r)
    {
      if (res.result() == http::status::unknown)
        res.result(http::status::ok);
    }

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
          {".woff2", "font/woff2"},
      };

      auto it = m.find(std::string(ext));
      return it == m.end() ? "application/octet-stream" : it->second;
    }

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
      return true;
    }

    ResponseWrapper &file(std::filesystem::path p)
    {
      ensure_status();

      const std::string s = p.lexically_normal().generic_string();
      if (s.find("..") != std::string::npos)
        return status(400).text("Bad path");

      std::error_code ec;
      if (std::filesystem::is_directory(p, ec))
      {
        p /= "index.html";
      }

      if (!std::filesystem::exists(p, ec) || !std::filesystem::is_regular_file(p, ec))
        return status(404).text("Not Found");

      std::string body;
      if (!read_file_binary(p, body))
        return status(500).text("File read error");

      std::string ext = p.extension().string();
      if (!ext.empty())
      {
        // lower-case
        for (char &c : ext)
          c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
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
      res.set("X-Content-Type-Options", "nosniff");

      // cache-control
      if (!has_header(http::field::cache_control))
        header("Cache-Control", "public, max-age=3600");

      res.body() = std::move(body);
      res.prepare_payload();
      return *this;
    }

    void ensure_status() noexcept
    {
      if (res.result() == http::status::unknown)
        res.result(http::status::ok);
    }

    bool has_header(http::field f) const
    {
      return res.find(f) != res.end();
    }

    bool has_body() const
    {
      return !res.body().empty();
    }

    static std::string default_status_message(int code)
    {
      auto s = vix::vhttp::status_to_string(code);
      return s;
    }

    ResponseWrapper &status(http::status code) noexcept
    {
      res.result(code);
      return *this;
    }

    ResponseWrapper &set_status(http::status code) noexcept { return status(code); }

    ResponseWrapper &status(int code)
    {
      if (code < 100 || code > 599)
      {
#ifndef NDEBUG
        throw std::runtime_error(
            "Invalid HTTP status code: " + std::to_string(code) +
            ". Status code must be between 100 and 599.");
#else
        res.result(http::status::internal_server_error);
        return *this;
#endif
      }

      res.result(vix::vhttp::to_status(code));
      return *this;
    }

    ResponseWrapper &set_status(int code) { return status(code); }

    template <int Code>
    ResponseWrapper &status_c() noexcept
    {
      static_assert(Code >= 100 && Code <= 599, "HTTP status code must be in [100..599]");
      res.result(static_cast<http::status>(Code));
      return *this;
    }

    template <int Code>
    ResponseWrapper &set_status_c() noexcept { return status_c<Code>(); }

    ResponseWrapper &header(std::string_view key, std::string_view value)
    {
      res.set(boost::beast::string_view{key.data(), key.size()},
              boost::beast::string_view{value.data(), value.size()});
      return *this;
    }

    ResponseWrapper &set(std::string_view key, std::string_view value) { return header(key, value); }

    ResponseWrapper &append(std::string_view key, std::string_view value)
    {
      boost::beast::string_view k{key.data(), key.size()};
      boost::beast::string_view v{value.data(), value.size()};

      auto it = res.find(k);
      if (it == res.end())
      {
        res.insert(k, v);
        return *this;
      }

      std::string combined = std::string(it->value());
      if (!combined.empty())
        combined += ", ";
      combined.append(v.data(), v.size());

      res.set(k, combined);
      return *this;
    }

    ResponseWrapper &type(std::string_view mime)
    {
      res.set(http::field::content_type,
              boost::beast::string_view{mime.data(), mime.size()});
      return *this;
    }

    ResponseWrapper &contentType(std::string_view mime) { return type(mime); }

    ResponseWrapper &redirect(std::string_view url)
    {
      return redirect(http::status::found, url);
    }

    ResponseWrapper &redirect(http::status code, std::string_view url)
    {
      status(code);
      header("Location", url);

      if (!has_header(http::field::content_type))
      {
        type("text/html; charset=utf-8");
        res.set("X-Content-Type-Options", "nosniff");
      }

      std::string body;
      body.reserve(256);
      body += "<!doctype html><html><head><meta charset=\"utf-8\"></head><body>";
      body += "Redirecting to ";
      body += std::string(url);
      body += "</body></html>";

      vix::vhttp::Response::text_response(res, body, res.result());
      return *this;
    }

    ResponseWrapper &redirect(int code, std::string_view url)
    {
      status(code);
      return redirect(res.result(), url);
    }

    ResponseWrapper &sendStatus(int code)
    {
      status(code);

      const int s = static_cast<int>(res.result());
      if (s == 204 || s == 304)
        return this->send();
      return send(default_status_message(s));
    }

    ResponseWrapper &text(std::string_view data)
    {
      ensure_status();

      const int s = static_cast<int>(res.result());
      if (s == 204 || s == 304)
        return this->send();

      if (!has_header(http::field::content_type))
      {
        type("text/plain; charset=utf-8");
        res.set("X-Content-Type-Options", "nosniff");
      }

      vix::vhttp::Response::text_response(res, data, res.result());
      return *this;
    }

    ResponseWrapper &json(const nlohmann::json &j)
    {
      ensure_status();

      const int s = static_cast<int>(res.result());
      if (s == 204 || s == 304)
        return this->send();

      if (!has_header(http::field::content_type))
      {
        type("application/json; charset=utf-8");
        res.set("X-Content-Type-Options", "nosniff");
      }

      vix::vhttp::Response::json_response(res, j, res.result());
      return *this;
    }

    ResponseWrapper &json(const vix::json::kvs &kv)
    {
      auto j = kvs_to_nlohmann(kv);
      return json(j);
    }

    ResponseWrapper &json(std::initializer_list<vix::json::token> list)
    {
      return json(vix::json::kvs{list});
    }

    ResponseWrapper &json_ordered(const OrderedJson &j)
    {
      ensure_status();

      const int s = static_cast<int>(res.result());
      if (s == 204 || s == 304)
        return this->send();

      if (!has_header(http::field::content_type))
      {
        type("application/json; charset=utf-8");
        res.set("X-Content-Type-Options", "nosniff");
      }

      vix::vhttp::ordered_json_response(res, j, res.result());
      return *this;
    }

    template <typename J>
      requires(!std::is_same_v<std::decay_t<J>, nlohmann::json> &&
               !std::is_same_v<std::decay_t<J>, vix::json::kvs> &&
               !std::is_same_v<std::decay_t<J>, std::initializer_list<vix::json::token>> &&
               !std::is_same_v<std::decay_t<J>, OrderedJson>)
    ResponseWrapper &json(const J &data)
    {
      ensure_status();

      const int s = static_cast<int>(res.result());
      if (s == 204 || s == 304)
        return this->send();

      if (!has_header(http::field::content_type))
      {
        type("application/json; charset=utf-8");
        res.set("X-Content-Type-Options", "nosniff");
      }

      vix::vhttp::Response::json_response(res, data, res.result());
      return *this;
    }

    ResponseWrapper &send()
    {
      ensure_status();

      const int s = static_cast<int>(res.result());
      if (s == 204 || s == 304)
      {
        res.body().clear();
        res.prepare_payload();
        return *this;
      }

      if (!has_body())
      {
        return text(default_status_message(s));
      }

      res.prepare_payload();
      return *this;
    }

    ResponseWrapper &end()
    {
      return send();
    }

    // res.location("/login").status(401).send()
    // res.location("/dashboard").status(302).send()
    ResponseWrapper &location(std::string_view url)
    {
      return header("location", url);
    }

    ResponseWrapper &send(std::string_view data) { return text(data); }
    ResponseWrapper &send(const char *data) { return text(data ? std::string_view{data} : std::string_view{}); }
    ResponseWrapper &send(const std::string &data) { return text(std::string_view{data}); }

    ResponseWrapper &send(const nlohmann::json &j) { return json(j); }
    ResponseWrapper &send(const vix::json::kvs &kv) { return json(kv); }
    ResponseWrapper &send(std::initializer_list<vix::json::token> list) { return json(list); }
    ResponseWrapper &send(const OrderedJson &j) { return json_ordered(j); }

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

    template <typename T>
    ResponseWrapper &send(int statusCode, const T &payload)
    {
      status(statusCode);
      return send(payload);
    }

    template <int Code, typename T>
    ResponseWrapper &send_c(const T &payload)
    {
      status_c<Code>();
      return send(payload);
    }

    ResponseWrapper &ok() { return status(http::status::ok); }
    ResponseWrapper &created() { return status(http::status::created); }
    ResponseWrapper &accepted() { return status(http::status::accepted); }
    ResponseWrapper &no_content() { return status(http::status::no_content); }
    ResponseWrapper &bad_request() { return status(http::status::bad_request); }
    ResponseWrapper &unauthorized() { return status(http::status::unauthorized); }
    ResponseWrapper &forbidden() { return status(http::status::forbidden); }
    ResponseWrapper &not_found() { return status(http::status::not_found); }
    ResponseWrapper &conflict() { return status(http::status::conflict); }
    ResponseWrapper &internal_error() { return status(http::status::internal_server_error); }
    ResponseWrapper &not_implemented() { return status(http::status::not_implemented); }
    ResponseWrapper &bad_gateway() { return status(http::status::bad_gateway); }
    ResponseWrapper &service_unavailable() { return status(http::status::service_unavailable); }
  };
}

#endif
