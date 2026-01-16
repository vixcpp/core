/**
 *
 *  @file result.hpp
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
#ifndef VIX_RESULT_HPP
#define VIX_RESULT_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

#include <nlohmann/json.hpp>

namespace vix::mw
{
  struct Error final
  {
    int status{500};
    std::string code{"internal_server_error"};
    std::string message{"Internal Server Error"};
    std::unordered_map<std::string, std::string> details{};
  };

  inline nlohmann::json to_json(const Error &e)
  {
    nlohmann::json j;
    j["status"] = e.status;
    j["code"] = e.code;
    j["message"] = e.message;
    if (!e.details.empty())
      j["details"] = e.details;
    return j;
  }

  template <class T>
  class Result final
  {
  public:
    using value_type = T;

    Result(const Result &) = default;
    Result &operator=(const Result &) = default;
    Result(Result &&) noexcept = default;
    Result &operator=(Result &&) noexcept = default;
    ~Result() = default;

    static Result ok(T value)
    {
      return Result(std::move(value));
    }

    static Result fail(Error err)
    {
      return Result(std::move(err));
    }

    bool is_ok() const noexcept
    {
      return std::holds_alternative<T>(data_);
    }

    bool is_err() const noexcept
    {
      return std::holds_alternative<Error>(data_);
    }

    explicit operator bool() const noexcept { return is_ok(); }

    T &value()
    {
      return std::get<T>(data_);
    }

    const T &value() const
    {
      return std::get<T>(data_);
    }

    Error &error()
    {
      return std::get<Error>(data_);
    }

    const Error &error() const
    {
      return std::get<Error>(data_);
    }

    T value_or(T fallback) const
    {
      return is_ok() ? std::get<T>(data_) : std::move(fallback);
    }

  private:
    explicit Result(T value)
        : data_(std::move(value))
    {
    }

    explicit Result(Error err)
        : data_(std::move(err))
    {
    }

  private:
    std::variant<T, Error> data_;
  };

  template <>
  class Result<void> final
  {
  public:
    Result(const Result &) = default;
    Result &operator=(const Result &) = default;
    Result(Result &&) noexcept = default;
    Result &operator=(Result &&) noexcept = default;
    ~Result() = default;

    static Result ok()
    {
      return Result(true, Error{});
    }

    static Result fail(Error err)
    {
      return Result(false, std::move(err));
    }

    bool is_ok() const noexcept { return ok_; }
    bool is_err() const noexcept { return !ok_; }

    explicit operator bool() const noexcept { return ok_; }

    Error &error() { return err_; }
    const Error &error() const { return err_; }

  private:
    Result(bool ok, Error err)
        : ok_(ok), err_(std::move(err))
    {
    }

  private:
    bool ok_{true};
    Error err_{};
  };

  template <class T>
  inline Result<T> ok(T v)
  {
    return Result<T>::ok(std::move(v));
  }

  inline Result<void> ok()
  {
    return Result<void>::ok();
  }

  template <class T = void>
  inline Result<T> fail(
      int status,
      std::string code,
      std::string message,
      std::unordered_map<std::string, std::string> details = {})
  {
    Error e;
    e.status = status;
    e.code = std::move(code);
    e.message = std::move(message);
    e.details = std::move(details);
    return Result<T>::fail(std::move(e));
  }

  template <class T = void>
  inline Result<T> fail(Error e)
  {
    return Result<T>::fail(std::move(e));
  }

  inline Error bad_request(std::string message = "Bad Request")
  {
    return Error{400, "bad_request", std::move(message), {}};
  }

  inline Error unauthorized(std::string message = "Unauthorized")
  {
    return Error{401, "unauthorized", std::move(message), {}};
  }

  inline Error forbidden(std::string message = "Forbidden")
  {
    return Error{403, "forbidden", std::move(message), {}};
  }

  inline Error not_found(std::string message = "Not Found")
  {
    return Error{404, "not_found", std::move(message), {}};
  }

  inline Error conflict(std::string message = "Conflict")
  {
    return Error{409, "conflict", std::move(message), {}};
  }

  inline Error too_many_requests(std::string message = "Too Many Requests")
  {
    return Error{429, "rate_limited", std::move(message), {}};
  }

  inline Error internal(std::string message = "Internal Server Error")
  {
    return Error{500, "internal_server_error", std::move(message), {}};
  }

  inline int clamp_http_status(int status) noexcept
  {
    if (status < 100)
      return 500;
    if (status > 599)
      return 500;
    return status;
  }

  inline Error normalize(Error e)
  {
    e.status = clamp_http_status(e.status);
    if (e.code.empty())
      e.code = "error";
    if (e.message.empty())
      e.message = "Error";
    return e;
  }

} // namespace vix::mw

#endif
