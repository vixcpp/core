#include <vix/session/Session.hpp>

namespace Vix
{
    using Level = Logger::Level;
    static Logger &logger = Logger::getInstance();

    const std::regex Session::XSS_PATTERN(R"(<script.*?>.*?</script>)", std::regex::icase);
    const std::regex Session::SQL_PATTERN(R"((\bUNION\b|\bSELECT\b|\bINSERT\b|\bDELETE\b|\bUPDATE\b|\bDROP\b))", std::regex::icase);

    Session::Session(std::shared_ptr<tcp::socket> socket, Router &router)
        : socket_(std::move(socket)), router_(router)
    {
        boost::system::error_code ec;
        socket_->set_option(tcp::no_delay(true), ec);
        if (ec)
            logger.log(Level::WARN, "[Session] Failed to disable Nagle: {}", ec.message());
    }

    void Session::run()
    {
        logger.log(Level::DEBUG, "[Session] Starting new session");
        read_request();
    }

    void Session::start_timer()
    {
        timer_ = std::make_shared<net::steady_timer>(socket_->get_executor());
        timer_->expires_after(REQUEST_TIMEOUT);

        std::weak_ptr<net::steady_timer> weak_timer = timer_;
        auto self = shared_from_this();
        timer_->async_wait([this, self, weak_timer](const boost::system::error_code &ec)
                           {
            auto t = weak_timer.lock();
            if (!t) return;

            if (!ec)
            {
                logger.log(Level::WARN, "[Session] Timeout ({}s), closing socket", REQUEST_TIMEOUT.count());
                close_socket_gracefully();
            } });
    }

    void Session::cancel_timer()
    {
        if (timer_)
        {
            boost::system::error_code ec;
            timer_->cancel(ec);
        }
    }

    void Session::read_request()
    {
        if (!socket_ || !socket_->is_open())
        {
            logger.log(Vix::Logger::Level::DEBUG, "[Session] Socket closed before read_request()");
            return;
        }

        buffer_.consume(buffer_.size());
        parser_ = std::make_unique<http::request_parser<http::string_body>>();
        parser_->body_limit(MAX_REQUEST_BODY_SIZE);

        start_timer();

        auto self = shared_from_this();
        http::async_read(
            *socket_, buffer_, *parser_,
            [this, self](boost::system::error_code ec, std::size_t)
            {
                cancel_timer();

                if (ec)
                {
                    if (ec == http::error::end_of_stream ||
                        ec == boost::asio::error::connection_reset)
                    {
                        logger.log(Vix::Logger::Level::DEBUG,
                                   "[Session] Client closed connection: {}", ec.message());
                    }
                    else if (ec != boost::asio::error::operation_aborted)
                    {
                        logger.log(Vix::Logger::Level::ERROR,
                                   "[Session] Read error: {}", ec.message());
                    }

                    close_socket_gracefully();
                    return;
                }

                std::optional<http::request<http::string_body>> parsed;
                try
                {
                    parsed = parser_->release();
                }
                catch (const std::exception &ex)
                {
                    logger.log(Vix::Logger::Level::ERROR,
                               "[Session] Parser release failed: {}", ex.what());
                    close_socket_gracefully();
                    return;
                }

                handle_request({}, std::move(parsed));
            });
    }

    void Session::handle_request(const boost::system::error_code &ec,
                                 std::optional<http::request<http::string_body>> parsed_req)
    {
        if (ec)
        {
            logger.log(Level::ERROR, "[Session] Error handling request: {}", ec.message());
            close_socket_gracefully();
            return;
        }

        if (!parsed_req)
        {
            logger.log(Level::WARN, "[Session] No request parsed");
            close_socket_gracefully();
            return;
        }

        req_ = std::move(*parsed_req);

        if (!waf_check_request(req_))
        {
            logger.log(Level::WARN, "[WAF] Request blocked by rules");
            send_error(http::status::bad_request, "Request blocked (security)");
            return;
        }

#if defined(BOOST_BEAST_VERSION) && BOOST_BEAST_VERSION >= 315
        constexpr auto too_large_status = http::status::payload_too_large;
#else
        constexpr auto too_large_status = static_cast<http::status>(413);
#endif
        if (req_.body().size() > MAX_REQUEST_BODY_SIZE)
        {
            logger.log(Level::WARN, "[Session] Body too large ({} bytes)", req_.body().size());
            send_error(too_large_status, "Request too large");
            return;
        }

        http::response<http::string_body> res;
        bool ok = false;
        try
        {
            ok = router_.handle_request(req_, res);
        }
        catch (const std::exception &ex)
        {
            logger.log(Level::ERROR, "[Router] Exception: {}", ex.what());
            send_error(http::status::internal_server_error, "Internal server error");
            return;
        }

        if (!ok)
        {
            if (res.result() == http::status::ok)
                res.result(http::status::bad_request);
        }

        res.set(http::field::connection, req_.keep_alive() ? "keep-alive" : "close");
        send_response(std::move(res));
    }

    void Session::send_response(http::response<http::string_body> res)
    {
        if (!socket_ || !socket_->is_open())
        {
            logger.log(Level::DEBUG, "[Session] Cannot send response (socket closed)");
            return;
        }

        auto self = shared_from_this();
        auto res_ptr = std::make_shared<http::response<http::string_body>>(std::move(res));

        http::async_write(*socket_, *res_ptr,
                          [this, self, res_ptr](boost::system::error_code ec, std::size_t)
                          {
                              if (ec)
                              {
                                  logger.log(Level::WARN, "[Session] Write error: {}", ec.message());
                                  close_socket_gracefully();
                                  return;
                              }

                              logger.log(Level::DEBUG, "[Session] Response sent ({} bytes)", res_ptr->body().size());

                              if (res_ptr->keep_alive())
                              {
                                  parser_.reset();
                                  read_request();
                              }
                              else
                                  close_socket_gracefully();
                          });
    }

    void Session::send_error(http::status status, const std::string &msg)
    {
        http::response<http::string_body> res;
        Response::error_response(res, status, msg);
        res.set(http::field::connection, "close");
        send_response(std::move(res));
    }

    void Session::close_socket_gracefully()
    {
        if (!socket_ || !socket_->is_open())
            return;

        boost::system::error_code ec;
        socket_->shutdown(tcp::socket::shutdown_both, ec);
        socket_->close(ec);

        logger.log(Level::DEBUG, "[Session] Socket closed");
    }

    bool Session::waf_check_request(const http::request<http::string_body> &req)
    {
        if (req.target().size() > 4096)
        {
            logger.log(Level::WARN, "[WAF] Target too long");
            return false;
        }

        try
        {
            if (std::regex_search(req.target().to_string(), XSS_PATTERN))
            {
                logger.log(Level::WARN, "[WAF] XSS pattern detected in URL");
                return false;
            }

            if (!req.body().empty() && std::regex_search(req.body(), SQL_PATTERN))
            {
                logger.log(Level::WARN, "[WAF] SQL injection attempt detected");
                return false;
            }
        }
        catch (const std::regex_error &)
        {
            logger.log(Level::ERROR, "[WAF] Regex error during pattern check");
            return false;
        }

        return true;
    }
} // namespace Vix
