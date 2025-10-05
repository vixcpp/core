#ifndef VIX_SESSION_HPP
#define VIX_SESSION_HPP

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio.hpp>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>
#include <memory>
#include <regex>

#include "../router/Router.hpp"
#include "../http/Response.hpp"

namespace Vix
{
    namespace http = boost::beast::http;
    namespace net = boost::asio;
    namespace beast = boost::beast;
    using tcp = net::ip::tcp;
    using json = nlohmann::json;

    constexpr size_t MAX_REQUEST_BODY_SIZE = 10 * 1024 * 1024; // 10 MB
    constexpr auto REQUEST_TIMEOUT = std::chrono::seconds(20);

    class Session : public std::enable_shared_from_this<Session>
    {
    public:
        explicit Session(std::shared_ptr<tcp::socket> socket, Router &router);
        ~Session() = default;

        void run();

    private:
        void read_request();
        void start_timer();
        void cancel_timer();
        void close_socket();
        void handle_request(const boost::system::error_code &ec);
        void send_response(http::response<http::string_body> res);
        void send_error(const std::string &error_message);
        bool waf_check_request(const http::request<http::string_body> &req);

        std::shared_ptr<tcp::socket> socket_;
        Router &router_;
        beast::flat_buffer buffer_;
        http::request<http::string_body> req_;
        std::shared_ptr<net::steady_timer> timer_;

        static const std::regex XSS_PATTERN;
        static const std::regex SQL_PATTERN;
    };
}

#endif // VIX_SESSION_HPP
