/**
 *
 *  @file Session.cpp
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
#include <vix/session/Session.hpp>
#include <vix/utils/Logger.hpp>
#include <cctype>

namespace vix::session
{
  using Logger = vix::utils::Logger;

  inline Logger &log()
  {
    return Logger::getInstance();
  }

  const std::regex Session::XSS_PATTERN(R"(<script.*?>.*?</script>)", std::regex::icase);
  const std::regex Session::SQL_PATTERN(R"((\bUNION\b|\bSELECT\b|\bINSERT\b|\bDELETE\b|\bUPDATE\b|\bDROP\b))", std::regex::icase);

  Session::Session(
      std::shared_ptr<tcp::socket> socket,
      vix::router::Router &router,
      const vix::config::Config &config,
      std::shared_ptr<vix::executor::IExecutor> executor)
      : socket_(std::move(socket)),
        router_(router),
        buffer_(),
        req_(),
        parser_(nullptr),
        timer_(nullptr),
        config_(config),
        executor_(std::move(executor)),
        strand_(socket_->get_executor())
  {
    boost::system::error_code ec;
    socket_->set_option(tcp::no_delay(true), ec);
    if (ec)
      log().log(Logger::Level::Warn, "[Session] Failed to disable Nagle: {}", ec.message());
  }

  void Session::send_response_strand(bhttp::response<bhttp::string_body> res)
  {
    auto self = shared_from_this();
    boost::asio::dispatch(
        strand_, [self, r = std::move(res)]() mutable
        { self->send_response(std::move(r)); });
  }

  void Session::run()
  {
    log().log(Logger::Level::Debug, "[Session] Starting new session");
    read_request();
  }

  void Session::start_timer()
  {
    timer_ = std::make_shared<net::steady_timer>(socket_->get_executor());
    const auto timeout = std::chrono::seconds(config_.getSessionTimeoutSec());
    timer_->expires_after(timeout);

    std::weak_ptr<net::steady_timer> weak_timer = timer_;
    auto self = shared_from_this();

    timer_->async_wait(
        [this, self, weak_timer, timeout](const boost::system::error_code &ec)
        {
          auto t = weak_timer.lock();
          if (!t)
            return;

          if (!ec)
          {
            log().log(Logger::Level::Warn,
                      "[Session] Timeout ({}s), closing socket",
                      timeout.count());
            close_socket_gracefully();
          }
        });
  }

  void Session::cancel_timer()
  {
    if (timer_)
    {
      const std::size_t n = timer_->cancel();
      (void)n;
    }
  }

  void Session::read_request()
  {
    if (!socket_ || !socket_->is_open())
    {
      log().log(Logger::Level::Debug, "[Session] Socket closed before read_request()");
      return;
    }

    buffer_.consume(buffer_.size());
    parser_ = std::make_unique<bhttp::request_parser<bhttp::string_body>>();
    parser_->body_limit(MAX_REQUEST_BODY_SIZE);

    start_timer();

    auto self = shared_from_this();
    bhttp::async_read(
        *socket_, buffer_, *parser_,
        [this, self](boost::system::error_code ec, std::size_t)
        {
          cancel_timer();

          if (ec)
          {
            if (ec == bhttp::error::end_of_stream ||
                ec == boost::asio::error::connection_reset)
            {
              log().log(Logger::Level::Debug,
                        "[Session] Client closed connection: {}", ec.message());
            }
            else if (ec != boost::asio::error::operation_aborted)
            {
              log().log(Logger::Level::Error,
                        "[Session] Read error: {}", ec.message());
            }

            close_socket_gracefully();
            return;
          }

          std::optional<bhttp::request<bhttp::string_body>> parsed;
          try
          {
            parsed = parser_->release();
          }
          catch (const std::exception &ex)
          {
            log().log(Logger::Level::Error,
                      "[Session] Parser release failed: {}", ex.what());
            close_socket_gracefully();
            return;
          }

          handle_request({}, std::move(parsed));
        });
  }

  void Session::handle_request(
      const boost::system::error_code &ec,
      std::optional<bhttp::request<bhttp::string_body>> parsed_req)
  {
    if (ec)
    {
      log().log(Logger::Level::Error, "[Session] Error handling request: {}", ec.message());
      close_socket_gracefully();
      return;
    }

    if (!parsed_req)
    {
      log().log(Logger::Level::Warn, "[Session] No request parsed");
      close_socket_gracefully();
      return;
    }

    req_ = std::move(*parsed_req);

    if (!waf_check_request(req_))
    {
      log().log(Logger::Level::Warn, "[WAF] Request blocked by rules");
      send_error(bhttp::status::bad_request, "Request blocked (security)");
      return;
    }

#if defined(BOOST_BEAST_VERSION) && BOOST_BEAST_VERSION >= 315
    constexpr auto too_large_status = http::status::payload_too_large;
#else
    constexpr auto too_large_status = static_cast<bhttp::status>(413);
#endif

    if (req_.body().size() > MAX_REQUEST_BODY_SIZE)
    {
      log().log(Logger::Level::Warn, "[Session] Body too large ({} bytes)", req_.body().size());
      send_error(too_large_status, "Request too large");
      return;
    }

    auto self = shared_from_this();
    auto req = std::move(req_);
    const bool heavy = router_.is_heavy(req);

    if (!heavy)
    {
      bhttp::response<bhttp::string_body> res;
      bool ok = false;

      try
      {
        ok = router_.handle_request(req, res);
      }
      catch (const std::exception &ex)
      {
        log().log(Logger::Level::Error, "[Router] Exception: {}", ex.what());
        send_error(bhttp::status::internal_server_error, "Internal server error");
        return;
      }

      if (!ok && res.result() == bhttp::status::ok)
        res.result(bhttp::status::bad_request);

      res.set(bhttp::field::connection, req.keep_alive() ? "keep-alive" : "close");

      send_response_strand(std::move(res));
      return;
    }

    if (!executor_)
    {
      send_error(bhttp::status::service_unavailable, "Executor not configured");
      return;
    }

    auto res_ptr = std::make_shared<bhttp::response<bhttp::string_body>>();

    const bool accepted = executor_->post(
        [self, req = std::move(req), res_ptr]() mutable
        {
          bool ok = false;

          try
          {
            ok = self->router_.handle_request(req, *res_ptr);
          }
          catch (const std::exception &ex)
          {
            vix::utils::Logger::getInstance().log(
                vix::utils::Logger::Level::Error,
                "[Router][heavy] Exception: {}", ex.what());

            res_ptr->result(bhttp::status::internal_server_error);
            vix::vhttp::Response::error_response(
                *res_ptr,
                bhttp::status::internal_server_error,
                "Internal server error");
          }

          if (!ok && res_ptr->result() == bhttp::status::ok)
            res_ptr->result(bhttp::status::bad_request);

          res_ptr->set(bhttp::field::connection, req.keep_alive() ? "keep-alive" : "close");

          self->send_response_strand(std::move(*res_ptr));
        });

    if (!accepted)
    {
      send_error(bhttp::status::service_unavailable, "Server busy");
      return;
    }
  }

  void Session::send_response(bhttp::response<bhttp::string_body> res)
  {
    if (!socket_ || !socket_->is_open())
    {
      log().log(Logger::Level::Debug, "[Session] Cannot send response (socket closed)");
      return;
    }

    auto self = shared_from_this();
    auto res_ptr = std::make_shared<bhttp::response<bhttp::string_body>>(std::move(res));

    bhttp::async_write(
        *socket_, *res_ptr,
        [this, self, res_ptr](boost::system::error_code ec, std::size_t)
        {
          if (ec)
          {
            log().log(Logger::Level::Warn, "[Session] Write error: {}", ec.message());
            close_socket_gracefully();
            return;
          }

          log().log(Logger::Level::Debug, "[Session] Response sent ({} bytes)", res_ptr->body().size());

          if (res_ptr->keep_alive())
          {
            parser_.reset();
            read_request();
          }
          else
          {
            close_socket_gracefully();
          }
        });
  }

  void Session::send_error(bhttp::status status, const std::string &msg)
  {
    bhttp::response<bhttp::string_body> res;
    vix::vhttp::Response::error_response(res, status, msg);
    res.set(bhttp::field::connection, "close");
    send_response(std::move(res));
  }

  void Session::close_socket_gracefully()
  {
    if (!socket_ || !socket_->is_open())
      return;

    boost::system::error_code ec;
    socket_->shutdown(tcp::socket::shutdown_both, ec);
    socket_->close(ec);

    log().log(Logger::Level::Debug, "[Session] Socket closed");
  }

  static inline bool icontains(std::string_view s, std::string_view needle)
  {
    if (needle.empty())
      return true;
    if (needle.size() > s.size())
      return false;

    for (size_t i = 0; i + needle.size() <= s.size(); ++i)
    {
      size_t j = 0;
      for (; j < needle.size(); ++j)
      {
        unsigned char a = (unsigned char)s[i + j];
        unsigned char b = (unsigned char)needle[j];
        if ((char)std::tolower(a) != (char)std::tolower(b))
          break;
      }
      if (j == needle.size())
        return true;
    }
    return false;
  }

  bool Session::waf_check_request(const bhttp::request<bhttp::string_body> &req)
  {
#ifdef VIX_BENCH_MODE
    (void)req;
    return true;
#else
    const std::string &mode = config_.getWafMode(); // "off"|"basic"|"strict"
    if (mode == "off")
      return true;

    const std::size_t maxTargetLen = (std::size_t)config_.getWafMaxTargetLen();
    const std::size_t maxBodyBytes = (std::size_t)config_.getWafMaxBodyBytes();

    if (req.target().size() > maxTargetLen)
      return false;

    for (char c : req.target())
    {
      if (c == '\0' || c == '\r' || c == '\n')
        return false;
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
          return false;
      }
      catch (...)
      {
        return false;
      }
    }

    const auto m = req.method();
    const bool mutating =
        (m == bhttp::verb::post || m == bhttp::verb::put ||
         m == bhttp::verb::patch || m == bhttp::verb::delete_);

    if (!mutating)
      return true;

    const std::string &body = req.body();
    if (body.empty())
      return true;

    if (body.size() > maxBodyBytes)
      return false;

    const bool cheap_trigger =
        body.find('<') != std::string::npos ||
        icontains(body, "union") || icontains(body, "select") ||
        icontains(body, "drop") || icontains(body, "insert") ||
        icontains(body, "delete") || icontains(body, "update");

    if (mode == "basic" && !cheap_trigger)
      return true;

    try
    {
      if (std::regex_search(body, SQL_PATTERN))
        return false;
      if (std::regex_search(body, XSS_PATTERN))
        return false;
    }
    catch (...)
    {
      return false;
    }

    return true;
#endif
  }

} // namespace vix
