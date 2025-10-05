#include "Session.hpp"

namespace Vix
{
    Session::Session(std::shared_ptr<tcp::socket> socket, Router &router)
        : socket_(std::move(socket)), router_(router)
    {
        socket_->set_option(tcp::no_delay(true));
    }

    void Session::run()
    {
        read_request();
    }

    void Session::start_timer()
    {
        timer_ = std::make_shared<net::steady_timer>(socket_->get_executor());
        timer_->expires_after(REQUEST_TIMEOUT);

        std::weak_ptr<net::steady_timer> weak_timer = timer_;
        timer_->async_wait([this, weak_timer](const boost::system::error_code &ec)
                           {
            auto t = weak_timer.lock();
            if (!t) return;

            if (!ec)
            {
                spdlog::warn("Timeout: No request received after {} seconds!", REQUEST_TIMEOUT.count());
                close_socket();
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
            spdlog::error("Socket is not open, cannot read request!");
            return;
        }

        buffer_.consume(buffer_.size());
        start_timer();

        auto self = shared_from_this();
        http::async_read(*socket_, buffer_, req_,
                         [this, self](boost::system::error_code ec, std::size_t)
                         {
                             cancel_timer();

                             if (ec)
                             {
                                 if (ec == http::error::end_of_stream)
                                     spdlog::info("Client closed the connection.");
                                 else if (ec != boost::asio::error::operation_aborted)
                                     spdlog::error("Error during async_read: {}", ec.message());

                                 close_socket();
                                 return;
                             }

                             handle_request(ec);
                         });
    }

    void Session::handle_request(const boost::system::error_code &ec)
    {
        if (ec)
        {
            spdlog::error("Error handling request: {}", ec.message());
            close_socket();
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
            send_error(res.body());
            return;
        }

        if (req_[http::field::connection] != "close")
            res.set(http::field::connection, "keep-alive");
        else
            res.set(http::field::connection, "close");

        send_response(std::move(res));
    }

    void Session::send_response(http::response<http::string_body> res)
    {
        if (!socket_ || !socket_->is_open())
        {
            spdlog::error("Socket is not open, cannot send response!");
            return;
        }

        auto self = shared_from_this();
        auto res_ptr = std::make_shared<http::response<http::string_body>>(std::move(res));

        http::async_write(*socket_, *res_ptr,
                          [this, self, res_ptr](boost::system::error_code ec, std::size_t)
                          {
                              if (ec)
                              {
                                  spdlog::error("Error sending response: {}", ec.message());
                                  close_socket();
                                  return;
                              }

                              spdlog::info("Response sent successfully.");

                              if (req_[http::field::connection] != "close")
                                  read_request();
                              else
                                  close_socket();
                          });
    }

    void Session::send_error(const std::string &error_message)
    {
        http::response<http::string_body> res;
        Response::error_response(res, http::status::bad_request, error_message);
        send_response(std::move(res));
    }

    void Session::close_socket()
    {
        if (!socket_ || !socket_->is_open())
            return;

        boost::system::error_code ec;
        socket_->shutdown(tcp::socket::shutdown_both, ec);

        if (ec && ec != boost::asio::error::not_connected)
            spdlog::warn("Error during socket shutdown: {}", ec.message());

        socket_->close(ec);
        if (ec)
            spdlog::warn("Error closing socket: {}", ec.message());
        else
            spdlog::info("Socket closed.");
    }

    bool Session::waf_check_request(const http::request<http::string_body> &req)
    {
        if (std::regex_search(req.target().to_string(), XSS_PATTERN))
        {
            spdlog::warn("Possible XSS attack detected in URL: {}", req.target());
            return false;
        }

        if (std::regex_search(req.body(), SQL_PATTERN))
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
} // namespace Vix
