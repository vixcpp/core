#ifndef VIX_SESSION_HPP
#define VIX_SESSION_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <regex>
#include <optional>
#include <chrono>

#include <vix/router/Router.hpp>
#include <vix/http/Response.hpp>
#include <vix/utils/Logger.hpp>

namespace Vix
{
    namespace http = boost::beast::http;
    namespace net = boost::asio;
    namespace beast = boost::beast;
    using tcp = net::ip::tcp;

    constexpr size_t MAX_REQUEST_BODY_SIZE = 10 * 1024 * 1024; // 10MB
    constexpr auto REQUEST_TIMEOUT = std::chrono::seconds(20);

    class Session : public std::enable_shared_from_this<Session>
    {
    public:
        explicit Session(std::shared_ptr<tcp::socket> socket, Router &router);
        ~Session() = default;

        void run();

    private:
        void start_timer();
        void cancel_timer();
        void read_request();
        void handle_request(const boost::system::error_code &ec,
                            std::optional<http::request<http::string_body>> parsed_req);
        void send_response(http::response<http::string_body> res);
        void send_error(http::status status, const std::string &msg);
        void close_socket_gracefully();
        bool waf_check_request(const http::request<http::string_body> &req);

        std::shared_ptr<tcp::socket> socket_;
        Router &router_;
        beast::flat_buffer buffer_;
        http::request<http::string_body> req_;
        std::unique_ptr<http::request_parser<http::string_body>> parser_;
        std::shared_ptr<net::steady_timer> timer_;

        static const std::regex XSS_PATTERN;
        static const std::regex SQL_PATTERN;
    };
}

#endif // VIX_SESSION_HPP
