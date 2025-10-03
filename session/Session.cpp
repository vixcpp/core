#include "Session.hpp"

namespace Vix
{

    Session::Session(tcp::socket socket, Router &router)
        : socket_(std::move(socket)), router_(router), buffer_(), req_()
    {
        socket_.set_option(tcp::no_delay(true));
    }

    Session::~Session() {}

    void Session::run()
    {
        auto self = shared_from_this();
        read_request();
    }

    void Session::read_request()
    {
        if (!socket_.is_open())
        {
            spdlog::error("Socket is not open, cannot read request!");
            return;
        }

        auto self = shared_from_this();
        buffer_.consume(buffer_.size());

        auto timer = std::make_shared<boost::asio::steady_timer>(socket_.get_executor());
        timer->expires_after(std::chrono::seconds(20));

        std::weak_ptr<boost::asio::steady_timer> weak_timer = timer;
        timer->async_wait([this, self, weak_timer](boost::system::error_code ec)
                          {
            auto timer = weak_timer.lock();
            if (!timer)
            {
                spdlog::info("Timer is no longer available.");
                return;
            }
    
            if (!ec)
            {
                spdlog::warn("Timeout: No request received after 5 seconds!");
                close_socket();
            } });

        http::async_read(
            socket_, buffer_, req_,
            [this, self, timer](boost::system::error_code ec, std::size_t)
            {
                timer->cancel();

                if (ec)
                {
                    if (ec == http::error::end_of_stream)
                    {
                        spdlog::info("Client closed the connection.");
                    }
                    else if (ec != boost::asio::error::operation_aborted)
                    {
                        spdlog::error("Error during async_read: {}", ec.message());
                    }
                    close_socket();
                    return;
                }

                if (req_[http::field::connection] != "close")
                {
                    http::response<http::string_body> res;
                    res.set(http::field::connection, "keep-alive");
                }

                handle_request(ec);
            });
    }

    void Session::handle_request(const boost::system::error_code &ec)
    {
        if (ec)
        {
            spdlog::error("Error handling request: {}", ec.message());
            return;
        }

        if (!waf_check_request(req_))
        {
            spdlog::warn("Request blocked by WAF.");
            send_error("Request blocked due to security policy");
            return;
        }

        if (req_.body().size() > MAX_REQUEST_BODY_SIZE)
        {
            spdlog::warn("Request too large: {} bytes", req_.body().size());
            send_error("Request too large");
            return;
        }

        http::response<http::string_body> res;
        bool success = router_.handle_request(req_, res);

        if (!success)
        {
            if (res.result() == http::status::method_not_allowed)
            {
                send_error("Method Not Allowed");
            }
            else if (res.result() == http::status::not_found)
            {
                send_error("Route Not Found");
            }
            else
            {
                send_error("Invalid request");
            }
            return;
        }

        send_response(res);
    }

    void Session::send_response(http::response<http::string_body> &res)
    {
        if (!socket_.is_open())
        {
            spdlog::error("Socket is not open, cannot send response!");
            return;
        }

        auto self = shared_from_this();
        auto res_ptr = std::make_shared<http::response<http::string_body>>(std::move(res));

        http::async_write(
            socket_, *res_ptr,
            [this, self, res_ptr](boost::system::error_code ec, std::size_t)
            {
                if (ec)
                {
                    spdlog::error("Error sending response: {}", ec.message());
                    close_socket();
                    return;
                }

                spdlog::info("Response sent successfully.");

                net::post(socket_.get_executor(), [this, self]()
                          { close_socket(); });
            });
    }

    void Session::send_error(const std::string &error_message)
    {
        http::response<http::string_body> res;
        Response::error_response(res, http::status::bad_request, error_message);

        send_response(res);
    }

    void Session::close_socket()
    {
        if (!socket_.is_open())
        {
            spdlog::warn("Socket already closed or not open.");
            return;
        }

        boost::system::error_code ec;

        socket_.shutdown(tcp::socket::shutdown_both, ec);
        if (ec && ec != boost::asio::error::not_connected)
        {
            spdlog::warn("Error during socket shutdown: {}", ec.message());
        }

        if (socket_.is_open())
        {
            socket_.close(ec);
            if (ec)
            {
                spdlog::warn("Error closing socket: {}", ec.message());
            }
            else
            {
                spdlog::info("Socket closed.");
            }
        }
    }

    bool Session::waf_check_request(const http::request<http::string_body> &req)
    {
        std::regex xss_pattern(R"(<script.*?>.*?</script>)", std::regex::icase);
        std::regex sql_pattern(R"((\bUNION\b|\bSELECT\b|\bINSERT\b|\bDELETE\b|\bUPDATE\b|\bDROP\b))", std::regex::icase);

        if (std::regex_search(req.target().to_string(), xss_pattern))
        {
            spdlog::warn("Possible XSS attack detected in URL: {}", req.target());
            return false;
        }

        if (std::regex_search(req.body(), sql_pattern))
        {
            spdlog::warn("Possible SQL injection detected in body: {}", req.body());
            return false;
        }

        if (req.body().size() > MAX_REQUEST_BODY_SIZE)
        {
            spdlog::warn("Request body too large: {} bytes", req.body().size());
            return false;
        }

        return true;
    }

}
