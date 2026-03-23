/**
 *
 * @file Session.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license
 * that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#include <vix/session/Session.hpp>

#include <cctype>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <vix/http/Response.hpp>
#include <vix/utils/Logger.hpp>

namespace vix::session
{
  using Logger = vix::utils::Logger;

  namespace
  {
    inline Logger &log()
    {
      return Logger::getInstance();
    }

    inline bool icontains(std::string_view s, std::string_view needle)
    {
      if (needle.empty())
      {
        return true;
      }

      if (needle.size() > s.size())
      {
        return false;
      }

      for (std::size_t i = 0; i + needle.size() <= s.size(); ++i)
      {
        std::size_t j = 0;
        for (; j < needle.size(); ++j)
        {
          const unsigned char a = static_cast<unsigned char>(s[i + j]);
          const unsigned char b = static_cast<unsigned char>(needle[j]);

          if (static_cast<char>(std::tolower(a)) !=
              static_cast<char>(std::tolower(b)))
          {
            break;
          }
        }

        if (j == needle.size())
        {
          return true;
        }
      }

      return false;
    }
  } // namespace

  const std::regex Session::XSS_PATTERN(
      R"(<script.*?>.*?</script>)",
      std::regex::icase);

  const std::regex Session::SQL_PATTERN(
      R"((\bUNION\b|\bSELECT\b|\bINSERT\b|\bDELETE\b|\bUPDATE\b|\bDROP\b))",
      std::regex::icase);

  Session::Session(std::shared_ptr<tcp::socket> socket,
                   vix::router::Router &router,
                   const vix::config::Config &config,
                   std::shared_ptr<vix::executor::RuntimeExecutor> executor)
      : socket_(std::move(socket)),
        router_(router),
        buffer_(),
        req_(),
        parser_(nullptr),
        timer_(std::make_shared<net::steady_timer>(socket_->get_executor())),
        config_(config),
        executor_(std::move(executor)),
        strand_(socket_->get_executor())
  {
    boost::system::error_code ec;
    socket_->set_option(tcp::no_delay(true), ec);
  }

  void Session::run()
  {
    read_request();
  }

  void Session::start_timer()
  {
    if (!timer_)
    {
      return;
    }

    const auto timeout =
        std::chrono::seconds(config_.getSessionTimeoutSec());

    timer_->expires_after(timeout);

    auto self = shared_from_this();

    timer_->async_wait(
        net::bind_executor(
            strand_,
            [this, self](const boost::system::error_code &ec)
            {
              if (!ec)
              {
                close_socket_gracefully();
              }
            }));
  }

  void Session::cancel_timer()
  {
    if (!timer_)
    {
      return;
    }

    boost::system::error_code ec;
    timer_->cancel(ec);
  }

  void Session::read_request()
  {
    if (!socket_ || !socket_->is_open())
    {
      return;
    }

    cancel_timer();
    start_timer();

    req_ = {};
    parser_ = std::make_unique<bhttp::request_parser<bhttp::string_body>>();
    parser_->body_limit(MAX_REQUEST_BODY_SIZE);

    auto self = shared_from_this();

    bhttp::async_read(
        *socket_,
        buffer_,
        *parser_,
        net::bind_executor(
            strand_,
            [this, self](boost::system::error_code ec, std::size_t)
            {
              cancel_timer();

              if (ec)
              {
                if (ec != bhttp::error::end_of_stream &&
                    ec != boost::asio::error::connection_reset &&
                    ec != boost::asio::error::operation_aborted)
                {
                  log().log(Logger::Level::Error,
                            "[session] read error: {}",
                            ec.message());
                }

                close_socket_gracefully();
                return;
              }

              std::optional<bhttp::request<bhttp::string_body>> parsed_req;

              try
              {
                parsed_req = parser_->release();
              }
              catch (const std::exception &ex)
              {
                log().log(Logger::Level::Error,
                          "[session] parser release failed: {}",
                          ex.what());
                close_socket_gracefully();
                return;
              }

              handle_request({}, std::move(parsed_req));
            }));
  }

  void Session::handle_request(
      const boost::system::error_code &ec,
      std::optional<bhttp::request<bhttp::string_body>> parsed_req)
  {
    if (ec)
    {
      log().log(Logger::Level::Error,
                "[session] request handling error: {}",
                ec.message());
      close_socket_gracefully();
      return;
    }

    if (!parsed_req)
    {
      close_socket_gracefully();
      return;
    }

    req_ = std::move(*parsed_req);

    if (!config_.isBenchMode() && !waf_check_request(req_))
    {
      send_error(bhttp::status::bad_request, "Request blocked (security)");
      return;
    }

#if defined(BOOST_BEAST_VERSION) && BOOST_BEAST_VERSION >= 315
    constexpr auto too_large_status = bhttp::status::payload_too_large;
#else
    constexpr auto too_large_status = static_cast<bhttp::status>(413);
#endif

    if (req_.body().size() > MAX_REQUEST_BODY_SIZE)
    {
      send_error(too_large_status, "Request too large");
      return;
    }

    dispatch_request(std::move(req_));
  }

  void Session::dispatch_request(bhttp::request<bhttp::string_body> req)
  {
    auto self = shared_from_this();
    const bool keep_alive = req.keep_alive();

    if (!executor_)
    {
      auto res_ptr =
          std::make_shared<bhttp::response<bhttp::string_body>>();

      bool ok = false;

      try
      {
        ok = router_.handle_request(req, *res_ptr);
      }
      catch (const std::exception &ex)
      {
        log().log(Logger::Level::Error,
                  "[router] inline exception: {}",
                  ex.what());

        vix::vhttp::Response::error_response(
            *res_ptr,
            bhttp::status::internal_server_error,
            "Internal server error");
      }

      if (!ok && res_ptr->result() == bhttp::status::ok)
      {
        res_ptr->result(bhttp::status::bad_request);
      }

      res_ptr->keep_alive(keep_alive);
      send_response(std::move(*res_ptr));
      return;
    }

    const bool submitted = executor_->submit(
        [this, self, req = std::move(req), keep_alive]() mutable -> vix::runtime::TaskResult
        {
          auto res_ptr =
              std::make_shared<bhttp::response<bhttp::string_body>>();

          bool ok = false;

          try
          {
            ok = router_.handle_request(req, *res_ptr);
          }
          catch (const std::exception &ex)
          {
            log().log(Logger::Level::Error,
                      "[router] runtime exception: {}",
                      ex.what());

            vix::vhttp::Response::error_response(
                *res_ptr,
                bhttp::status::internal_server_error,
                "Internal server error");
          }

          if (!ok && res_ptr->result() == bhttp::status::ok)
          {
            res_ptr->result(bhttp::status::bad_request);
          }

          res_ptr->keep_alive(keep_alive);

          boost::asio::post(
              strand_,
              [self, res_ptr]() mutable
              {
                self->send_response(std::move(*res_ptr));
              });

          return vix::runtime::TaskResult::complete;
        });

    if (!submitted)
    {
      send_error(bhttp::status::service_unavailable,
                 "Runtime unavailable");
    }
  }

  void Session::send_response(bhttp::response<bhttp::string_body> res)
  {
    if (!socket_ || !socket_->is_open())
    {
      return;
    }

    cancel_timer();
    start_timer();

    auto self = shared_from_this();
    auto res_ptr =
        std::make_shared<bhttp::response<bhttp::string_body>>(std::move(res));

    bhttp::async_write(
        *socket_,
        *res_ptr,
        net::bind_executor(
            strand_,
            [this, self, res_ptr](boost::system::error_code ec, std::size_t)
            {
              cancel_timer();

              if (ec)
              {
                if (ec != boost::asio::error::operation_aborted &&
                    ec != boost::asio::error::connection_reset &&
                    ec != boost::asio::error::broken_pipe)
                {
                  log().log(Logger::Level::Error,
                            "[session] write error: {}",
                            ec.message());
                }

                close_socket_gracefully();
                return;
              }

              if (res_ptr->keep_alive())
              {
                parser_.reset();
                read_request();
              }
              else
              {
                close_socket_gracefully();
              }
            }));
  }

  void Session::send_error(bhttp::status status, const std::string &msg)
  {
    auto self = shared_from_this();

    boost::asio::post(
        strand_,
        [self, status, msg]()
        {
          bhttp::response<bhttp::string_body> res;
          vix::vhttp::Response::error_response(res, status, msg);
          res.keep_alive(false);
          self->send_response(std::move(res));
        });
  }

  void Session::close_socket_gracefully()
  {
    cancel_timer();

    if (!socket_)
    {
      return;
    }

    boost::system::error_code ec;

    if (socket_->is_open())
    {
      socket_->shutdown(tcp::socket::shutdown_both, ec);
      ec.clear();
      socket_->close(ec);
    }
  }

  bool Session::waf_check_request(const bhttp::request<bhttp::string_body> &req)
  {
#ifdef VIX_BENCH_MODE
    (void)req;
    return true;
#else
    const std::string &mode = config_.getWafMode();

    if (mode == "off")
    {
      return true;
    }

    const std::size_t maxTargetLen =
        static_cast<std::size_t>(config_.getWafMaxTargetLen());

    const std::size_t maxBodyBytes =
        static_cast<std::size_t>(config_.getWafMaxBodyBytes());

    if (req.target().size() > maxTargetLen)
    {
      return false;
    }

    for (char c : req.target())
    {
      if (c == '\0' || c == '\r' || c == '\n')
      {
        return false;
      }
    }

    std::string_view target{req.target().data(), req.target().size()};

    const bool suspicious_url =
        target.find('<') != std::string_view::npos ||
        icontains(target, "script") ||
        icontains(target, "union") ||
        icontains(target, "select") ||
        icontains(target, "drop");

    if (suspicious_url)
    {
      try
      {
        const std::string t(target);
        if (std::regex_search(t, XSS_PATTERN))
        {
          return false;
        }
      }
      catch (...)
      {
        return false;
      }
    }

    const auto m = req.method();

    const bool mutating =
        (m == bhttp::verb::post ||
         m == bhttp::verb::put ||
         m == bhttp::verb::patch ||
         m == bhttp::verb::delete_);

    if (!mutating)
    {
      return true;
    }

    const std::string &body = req.body();

    if (body.empty())
    {
      return true;
    }

    if (body.size() > maxBodyBytes)
    {
      return false;
    }

    const bool cheap_trigger =
        body.find('<') != std::string::npos ||
        icontains(body, "union") ||
        icontains(body, "select") ||
        icontains(body, "drop") ||
        icontains(body, "insert") ||
        icontains(body, "delete") ||
        icontains(body, "update");

    if (mode == "basic" && !cheap_trigger)
    {
      return true;
    }

    try
    {
      if (std::regex_search(body, SQL_PATTERN))
      {
        return false;
      }

      if (std::regex_search(body, XSS_PATTERN))
      {
        return false;
      }
    }
    catch (...)
    {
      return false;
    }

    return true;
#endif
  }

} // namespace vix::session
