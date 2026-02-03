/**
 * @file Session.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 */

#ifndef VIX_SESSION_HPP
#define VIX_SESSION_HPP

#include <boost/asio/dispatch.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

#include <chrono>
#include <memory>
#include <optional>

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include <regex>
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <vix/config/Config.hpp>
#include <vix/executor/IExecutor.hpp>
#include <vix/http/Response.hpp>
#include <vix/router/Router.hpp>

namespace vix::session
{
  namespace bhttp = boost::beast::http;
  namespace net = boost::asio;
  namespace beast = boost::beast;
  using tcp = net::ip::tcp;

  /** @brief Maximum accepted HTTP request body size in bytes (default: 10 MB). */
  constexpr std::size_t MAX_REQUEST_BODY_SIZE = 10 * 1024 * 1024;

  /** @brief One client connection session: reads requests, applies basic checks, routes to handlers, and writes responses. */
  class Session : public std::enable_shared_from_this<Session>
  {
  public:
    /** @brief Create a session bound to a connected TCP socket and a router used to dispatch requests. */
    explicit Session(std::shared_ptr<tcp::socket> socket,
                     vix::router::Router &router,
                     const vix::config::Config &config,
                     std::shared_ptr<vix::executor::IExecutor> executor);

    /** @brief Destroy the session (the underlying socket is closed by the owner when needed). */
    ~Session() = default;

    /** @brief Start the session: initializes timers, reads the request and dispatches it to the router. */
    void run();

  private:
    /** @brief Start a per-request timeout timer (prevents stalled / slow connections). */
    void start_timer();

    /** @brief Cancel the active timeout timer (called once a request is completed or aborted). */
    void cancel_timer();

    /** @brief Begin asynchronous reading/parsing of the next HTTP request. */
    void read_request();

    /** @brief Handle a parsed request result (success or failure) from the async read operation. */
    void handle_request(const boost::system::error_code &ec,
                        std::optional<bhttp::request<bhttp::string_body>> parsed_req);

    /** @brief Write an HTTP response to the client (may use strand to serialize writes). */
    void send_response(bhttp::response<bhttp::string_body> res);

    /** @brief Send a standardized error response with a status and short message. */
    void send_error(bhttp::status status, const std::string &msg);

    /** @brief Gracefully shutdown and close the socket (best-effort). */
    void close_socket_gracefully();

    /** @brief Apply basic WAF checks (SQLi/XSS patterns); returns true if request should be accepted. */
    bool waf_check_request(const bhttp::request<bhttp::string_body> &req);

    /** @brief Send a response through the strand to ensure thread-safe serialized socket writes. */
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

} // namespace vix::session

#endif // VIX_SESSION_HPP
