/**
 *
 *  @file Session.hpp
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
#ifndef VIX_SESSION_HPP
#define VIX_SESSION_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/strand.hpp>
#include <memory>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif

#include <regex>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <optional>
#include <chrono>

#include <vix/router/Router.hpp>
#include <vix/http/Response.hpp>
#include <vix/config/Config.hpp>
#include <vix/executor/IExecutor.hpp>

namespace vix::session
{
  namespace bhttp = boost::beast::http;
  namespace net = boost::asio;
  namespace beast = boost::beast;
  using tcp = net::ip::tcp;

  constexpr size_t MAX_REQUEST_BODY_SIZE = 10 * 1024 * 1024;
  constexpr auto REQUEST_TIMEOUT = std::chrono::seconds(20);

  class Session : public std::enable_shared_from_this<Session>
  {
  public:
    explicit Session(
        std::shared_ptr<tcp::socket> socket,
        vix::router::Router &router,
        const vix::config::Config &config,
        std::shared_ptr<vix::executor::IExecutor> executor);
    ~Session() = default;
    void run();

  private:
    void start_timer();
    void cancel_timer();
    void read_request();
    void handle_request(
        const boost::system::error_code &ec,
        std::optional<bhttp::request<bhttp::string_body>> parsed_req);
    void send_response(bhttp::response<bhttp::string_body> res);
    void send_error(bhttp::status status, const std::string &msg);
    void close_socket_gracefully();
    bool waf_check_request(const bhttp::request<bhttp::string_body> &req);
    void send_response_strand(bhttp::response<bhttp::string_body> res);

  private:
    std::shared_ptr<tcp::socket> socket_;
    vix::router::Router &router_;
    beast::flat_buffer buffer_;
    bhttp::request<bhttp::string_body> req_;
    std::unique_ptr<bhttp::request_parser<bhttp::string_body>> parser_;
    std::shared_ptr<net::steady_timer> timer_;
    static const std::regex XSS_PATTERN;
    static const std::regex SQL_PATTERN;
    const vix::config::Config &config_;
    std::shared_ptr<vix::executor::IExecutor> executor_;
    net::strand<net::any_io_executor> strand_;
  };

} // namespace vix

#endif // VIX_SESSION_HPP
