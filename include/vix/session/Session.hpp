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

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <vector>

#include <vix/async/core/cancel.hpp>
#include <vix/async/core/io_context.hpp>
#include <vix/async/core/task.hpp>
#include <vix/async/core/timer.hpp>
#include <vix/async/net/tcp.hpp>
#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/router/Router.hpp>

namespace vix::session
{
  using vix::async::core::cancel_source;
  using vix::async::core::io_context;
  using vix::async::core::task;
  using vix::async::net::tcp_stream;

  /** @brief Maximum accepted HTTP request body size in bytes (default: 10 MB). */
  constexpr std::size_t MAX_REQUEST_BODY_SIZE = 10 * 1024 * 1024;

  /**
   * @brief Minimal parsed HTTP request head produced by the native Vix parser.
   */
  struct ParsedRequestHead
  {
    std::string method{};
    std::string target{};
    std::string version{"HTTP/1.1"};
    std::unordered_map<std::string, std::string> headers{};
    std::size_t content_length{0};
    bool keep_alive{true};
  };

  /**
   * @brief One client connection session.
   *
   * This native Vix session:
   * - reads raw bytes from a Vix tcp_stream
   * - parses HTTP/1.1 requests
   * - applies basic request validation / WAF checks
   * - dispatches to the router
   * - serializes and writes native Vix responses
   */
  class Session : public std::enable_shared_from_this<Session>
  {
  public:
    /**
     * @brief Create a session bound to a connected native Vix TCP stream.
     *
     * @param stream Connected TCP stream.
     * @param router Router used to dispatch the HTTP request.
     * @param config Server configuration.
     * @param executor Runtime-backed executor used for handler execution.
     */
    explicit Session(std::unique_ptr<tcp_stream> stream,
                     vix::router::Router &router,
                     const vix::config::Config &config,
                     std::shared_ptr<vix::executor::RuntimeExecutor> executor);

    /** @brief Destroy the session. */
    ~Session() = default;

    /** @brief Start the session lifecycle. */
    task<void> run();

  private:
    /** @brief Start a per-request timeout scope. */
    void start_timer();

    /** @brief Cancel the active timeout scope. */
    void cancel_timer();

    /** @brief Read and parse the next HTTP request from the socket. */
    task<std::optional<vix::vhttp::Request>> read_request();

    /**
     * @brief Dispatch one parsed request to the runtime and router.
     *
     * @param req Parsed native Vix HTTP request.
     */
    task<void> dispatch_request(vix::vhttp::Request req);

    /**
     * @brief Write a native Vix HTTP response to the client.
     *
     * @param res Response to send.
     */
    task<void> send_response(vix::vhttp::Response res);

    /**
     * @brief Send a standardized HTTP error response.
     *
     * @param status HTTP status code.
     * @param msg Short error message.
     */
    task<void> send_error(int status, const std::string &msg);

    /** @brief Gracefully shutdown and close the stream. */
    task<void> close_stream_gracefully();

    /**
     * @brief Apply basic WAF checks to the request.
     *
     * @param req Incoming request.
     * @return true if request is accepted, false otherwise.
     */
    bool waf_check_request(const vix::vhttp::Request &req);

    /**
     * @brief Read raw bytes until the end of the HTTP header section.
     *
     * The returned string includes the terminating CRLF CRLF sequence.
     */
    task<std::string> read_header_block();

    /**
     * @brief Parse a raw HTTP header block into a ParsedRequestHead.
     *
     * Throws std::runtime_error on malformed input.
     */
    ParsedRequestHead parse_request_head(const std::string &raw_header) const;

    /**
     * @brief Read an HTTP request body according to the parsed request head.
     */
    task<std::string> read_request_body(const ParsedRequestHead &head);

    /**
     * @brief Build a native Vix Request from parsed head + body.
     */
    vix::vhttp::Request make_request(ParsedRequestHead head, std::string body);

    /**
     * @brief Return a lowercase copy of a string.
     */
    static std::string to_lower(std::string s);

    /**
     * @brief Trim leading/trailing ASCII whitespace.
     */
    static std::string trim(std::string s);

    /**
     * @brief Parse Content-Length safely.
     */
    static std::size_t parse_content_length(const std::string &value);

    /**
     * @brief Return true if the HTTP method usually allows a request body.
     */
    static bool method_allows_body(const std::string &method);

    /**
     * @brief Return true if the connection should be kept alive after this request.
     */
    static bool compute_keep_alive(const ParsedRequestHead &head);

  private:
    std::unique_ptr<tcp_stream> stream_;
    vix::router::Router &router_;
    const vix::config::Config &config_;
    std::shared_ptr<vix::executor::RuntimeExecutor> executor_;

    std::string read_buffer_{};
    std::shared_ptr<io_context> io_context_{};
    cancel_source timer_cancel_{};

    static const std::regex XSS_PATTERN;
    static const std::regex SQL_PATTERN;
  };

} // namespace vix::session

#endif // VIX_SESSION_HPP
