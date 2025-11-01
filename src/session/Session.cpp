#include <vix/session/Session.hpp>

/**
 * @file Session.cpp
 * @brief Implementation notes for vix::Session (maintainers-focused docs).
 *
 * Responsibilities covered here:
 *  - Socket options and timer lifecycle
 *  - Request parsing, error mapping, and size limits
 *  - WAF checks (XSS / SQLi) before dispatch
 *  - Keep‑alive vs. close semantics
 *  - Exception boundaries around Router handlers
 */

namespace vix::session
{
    using Logger = vix::utils::Logger;
    static Logger &logger = Logger::getInstance();

    // --- Regex rules for the lightweight WAF ---
    // Intentionally conservative, aimed at catching trivial exploit payloads
    // without deep parsing. Keep these in sync with docs and tests.
    const std::regex Session::XSS_PATTERN(R"(<script.*?>.*?</script>)", std::regex::icase);
    const std::regex Session::SQL_PATTERN(R"((\bUNION\b|\bSELECT\b|\bINSERT\b|\bDELETE\b|\bUPDATE\b|\bDROP\b))", std::regex::icase);

    /**
     * @brief Configure the accepted socket and initialize members.
     *
     * - Disables Nagle (TCP_NODELAY) to reduce latency for small responses.
     * - Parser and timer objects are created lazily per‑request.
     */
    Session::Session(std::shared_ptr<tcp::socket> socket, vix::router::Router &router)
        : socket_(std::move(socket)), router_(router),
          buffer_(),
          req_(),
          parser_(nullptr),
          timer_(nullptr)
    {
        boost::system::error_code ec;
        socket_->set_option(tcp::no_delay(true), ec);
        if (ec)
            logger.log(Logger::Level::WARN, "[Session] Failed to disable Nagle: {}", ec.message());
    }

    /**
     * @brief Entry point: kick off the first async read.
     */
    void Session::run()
    {
        logger.log(Logger::Level::DEBUG, "[Session] Starting new session");
        read_request();
    }

    /**
     * @brief Arm a per‑request timeout.
     *
     * We use a weak_ptr to avoid accessing a canceled/destroyed timer on late
     * completions. On expiration (no error), the socket is closed gracefully.
     */
    void Session::start_timer()
    {
        timer_ = std::make_shared<net::steady_timer>(socket_->get_executor());
        timer_->expires_after(REQUEST_TIMEOUT);

        std::weak_ptr<net::steady_timer> weak_timer = timer_;
        auto self = shared_from_this();
        timer_->async_wait([this, self, weak_timer](const boost::system::error_code &ec)
                           {
            auto t = weak_timer.lock();
            if (!t) return; // timer already destroyed

            if (!ec)
            {
                logger.log(Logger::Level::WARN, "[Session] Timeout ({}s), closing socket", REQUEST_TIMEOUT.count());
                close_socket_gracefully();
            } });
    }

    /** @brief Cancel an active timer; ignore benign errors. */
    void Session::cancel_timer()
    {
        if (timer_)
        {
            boost::system::error_code ec;
            timer_->cancel(ec);
        }
    }

    /**
     * @brief Prepare parser, enforce body limit, and read the request.
     *
     * On read completion:
     *  - Maps connection errors (EOF/reset) to DEBUG; other errors to ERROR.
     *  - Releases the parsed request from Beast's parser and defers to
     *    handle_request() for dispatch.
     */
    void Session::read_request()
    {
        if (!socket_ || !socket_->is_open())
        {
            logger.log(Logger::Level::DEBUG, "[Session] Socket closed before read_request()");
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
                        logger.log(Logger::Level::DEBUG,
                                   "[Session] Client closed connection: {}", ec.message());
                    }
                    else if (ec != boost::asio::error::operation_aborted)
                    {
                        logger.log(Logger::Level::ERROR,
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
                    logger.log(Logger::Level::ERROR,
                               "[Session] Parser release failed: {}", ex.what());
                    close_socket_gracefully();
                    return;
                }

                handle_request({}, std::move(parsed));
            });
    }

    /**
     * @brief Perform WAF, size checks, and dispatch to the Router.
     *
     * - Rejects absent requests (safety) and over‑limit bodies.
     * - Applies simple WAF regexes on URL and body.
     * - Calls `router_.handle_request(req_, res)` under exception boundary.
     * - Forces a non‑OK result to an error if the router failed and left OK.
     * - Sets `Connection: keep-alive|close` and writes the response.
     */
    void Session::handle_request(const boost::system::error_code &ec,
                                 std::optional<bhttp::request<bhttp::string_body>> parsed_req)
    {
        if (ec)
        {
            logger.log(Logger::Level::ERROR, "[Session] Error handling request: {}", ec.message());
            close_socket_gracefully();
            return;
        }

        if (!parsed_req)
        {
            logger.log(Logger::Level::WARN, "[Session] No request parsed");
            close_socket_gracefully();
            return;
        }

        req_ = std::move(*parsed_req);

        if (!waf_check_request(req_))
        {
            logger.log(Logger::Level::WARN, "[WAF] Request blocked by rules");
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
            logger.log(Logger::Level::WARN, "[Session] Body too large ({} bytes)", req_.body().size());
            send_error(too_large_status, "Request too large");
            return;
        }

        bhttp::response<bhttp::string_body> res;
        bool ok = false;
        try
        {
            ok = router_.handle_request(req_, res);
        }
        catch (const std::exception &ex)
        {
            logger.log(Logger::Level::ERROR, "[Router] Exception: {}", ex.what());
            send_error(bhttp::status::internal_server_error, "Internal server error");
            return;
        }

        if (!ok)
        {
            // If a handler signaled failure but forgot to set an error status,
            // downgrade OK to 400 to avoid sending a misleading success.
            if (res.result() == bhttp::status::ok)
                res.result(bhttp::status::bad_request);
        }

        res.set(bhttp::field::connection, req_.keep_alive() ? "keep-alive" : "close");
        send_response(std::move(res));
    }

    /**
     * @brief Async write response and manage keep‑alive re‑arm.
     */
    void Session::send_response(bhttp::response<bhttp::string_body> res)
    {
        if (!socket_ || !socket_->is_open())
        {
            logger.log(Logger::Level::DEBUG, "[Session] Cannot send response (socket closed)");
            return;
        }

        auto self = shared_from_this();
        auto res_ptr = std::make_shared<bhttp::response<bhttp::string_body>>(std::move(res));

        bhttp::async_write(*socket_, *res_ptr,
                           [this, self, res_ptr](boost::system::error_code ec, std::size_t)
                           {
                               if (ec)
                               {
                                   logger.log(Logger::Level::WARN, "[Session] Write error: {}", ec.message());
                                   close_socket_gracefully();
                                   return;
                               }

                               logger.log(Logger::Level::DEBUG, "[Session] Response sent ({} bytes)", res_ptr->body().size());

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

    /**
     * @brief Build and send a JSON error response, then close.
     */
    void Session::send_error(bhttp::status status, const std::string &msg)
    {
        bhttp::response<bhttp::string_body> res;
        vix::vhttp::Response::error_response(res, status, msg);
        res.set(bhttp::field::connection, "close");
        send_response(std::move(res));
    }

    /**
     * @brief Shutdown both directions and close the socket, ignoring benign errors.
     */
    void Session::close_socket_gracefully()
    {
        if (!socket_ || !socket_->is_open())
            return;

        boost::system::error_code ec;
        socket_->shutdown(tcp::socket::shutdown_both, ec);
        socket_->close(ec);

        logger.log(Logger::Level::DEBUG, "[Session] Socket closed");
    }

    /**
     * @brief Minimal WAF: block suspiciously long URIs and trivial XSS/SQLi.
     *
     * @return true if request passes the checks; false otherwise.
     */
    bool Session::waf_check_request(const bhttp::request<bhttp::string_body> &req)
    {
        if (req.target().size() > 4096)
        {
            logger.log(Logger::Level::WARN, "[WAF] Target too long");
            return false;
        }

        try
        {
            const std::string target{req.target().data(), req.target().size()};
            if (std::regex_search(target, XSS_PATTERN))
            {
                logger.log(Logger::Level::WARN, "[WAF] XSS pattern detected in URL");
                return false;
            }

            if (!req.body().empty() && std::regex_search(req.body(), SQL_PATTERN))
            {
                logger.log(Logger::Level::WARN, "[WAF] SQL injection attempt detected");
                return false;
            }
        }
        catch (const std::regex_error &)
        {
            logger.log(Logger::Level::ERROR, "[WAF] Regex error during pattern check");
            return false;
        }

        return true;
    }

} // namespace vix
