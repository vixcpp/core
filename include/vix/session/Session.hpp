#ifndef VIX_SESSION_HPP
#define VIX_SESSION_HPP

/**
 * @file Session.hpp
 * @brief Connection-level handler bridging Boost.Beast I/O with the Vix Router.
 *
 * @details
 * A `vix::Session` instance encapsulates the full lifecycle of a single HTTP
 * connection (usually one TCP socket accepted by `HTTPServer`). It is
 * responsible for:
 *
 *  1. Reading and parsing the HTTP request (using Boost.Beast).
 *  2. Performing lightweight security checks (regex-based WAF filters).
 *  3. Dispatching the request to the `Router` for route matching and handler
 *     execution.
 *  4. Sending the HTTP response (or an error) back to the client.
 *  5. Managing connection timeout and graceful socket closure.
 *
 * The `Session` is reference-counted (`enable_shared_from_this`) to safely
 * manage asynchronous operations without premature destruction.
 *
 * ### Lifecycle
 * ```text
 * run() → read_request() → handle_request() → send_response() → close_socket_gracefully()
 * ```
 *
 * ### Security filters
 * Basic input sanitation via regex patterns:
 * - `XSS_PATTERN`: detects potential cross-site scripting payloads.
 * - `SQL_PATTERN`: detects common SQL injection signatures.
 *
 * These checks are performed in `waf_check_request()` before dispatching the
 * request to the router. The intent is to mitigate trivial attacks early.
 *
 * ### Timeout handling
 * - A steady timer (`boost::asio::steady_timer`) enforces a global per-request
 *   timeout (`REQUEST_TIMEOUT`, default 20s).
 * - The timer is started when reading begins and canceled once a response is
 *   sent.
 * - On timeout, the connection is closed gracefully.
 *
 * ### Example
 * @code{.cpp}
 * // Inside HTTPServer::handle_client()
 * auto session = std::make_shared<vix::Session>(socket, router);
 * session->run();
 * @endcode
 */

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

namespace vix::session
{
    namespace bhttp = boost::beast::http;
    namespace net = boost::asio;
    namespace beast = boost::beast;
    using tcp = net::ip::tcp;

    /// Maximum accepted body size per request (10 MB).
    constexpr size_t MAX_REQUEST_BODY_SIZE = 10 * 1024 * 1024;

    /// Default timeout applied to request lifecycle (20 seconds).
    constexpr auto REQUEST_TIMEOUT = std::chrono::seconds(20);

    /**
     * @class Session
     * @brief Asynchronous handler for one HTTP connection.
     *
     * A Session owns its socket and coordinates I/O, WAF checks, routing, and
     * response emission. Each accepted socket corresponds to one Session
     * object.
     */
    class Session : public std::enable_shared_from_this<Session>
    {
    public:
        /**
         * @brief Construct a new Session.
         * @param socket Accepted TCP socket (shared ownership).
         * @param router Reference to the global router used for dispatch.
         */
        explicit Session(std::shared_ptr<tcp::socket> socket, vix::router::Router &router);

        /** @brief Destructor (defaulted). Connections are expected to self-close. */
        ~Session() = default;

        /**
         * @brief Entry point: start reading and processing the request.
         *
         * Initializes the timer, begins asynchronous read, and manages lifetime
         * through shared_ptr semantics.
         */
        void run();

    private:
        /** @brief Start the request timeout timer. */
        void start_timer();

        /** @brief Cancel any active timeout timer. */
        void cancel_timer();

        /** @brief Begin reading the HTTP request asynchronously. */
        void read_request();

        /**
         * @brief Process the parsed request or handle any read error.
         * @param ec          Result of asynchronous read.
         * @param parsed_req  Parsed HTTP request (optional; may be nullopt if parsing failed).
         *
         * - Performs WAF validation via `waf_check_request()`.
         * - Dispatches the request to the router.
         * - Catches and logs any exceptions raised by route handlers.
         */
        void handle_request(const boost::system::error_code &ec,
                            std::optional<bhttp::request<bhttp::string_body>> parsed_req);

        /**
         * @brief Send a serialized HTTP response.
         * @param res Response to write back to the client.
         */
        void send_response(bhttp::response<bhttp::string_body> res);

        /**
         * @brief Send an error response (JSON-encoded message).
         * @param status HTTP status code.
         * @param msg    Error message to return.
         */
        void send_error(bhttp::status status, const std::string &msg);

        /** @brief Close the socket gracefully (shutdown+close). */
        void close_socket_gracefully();

        /**
         * @brief Run simple regex-based Web Application Firewall checks.
         * @param req HTTP request to inspect.
         * @return true if the request passes validation; false if rejected.
         * @details Scans request body and target URI for suspicious patterns
         *          (XSS or SQLi). Blocks obvious injections early.
         */
        bool waf_check_request(const bhttp::request<bhttp::string_body> &req);

    private:
        std::shared_ptr<tcp::socket> socket_;                               //!< Underlying TCP connection.
        vix::router::Router &router_;                                       //!< Router reference for dispatch.
        beast::flat_buffer buffer_;                                         //!< Read buffer for Beast.
        bhttp::request<bhttp::string_body> req_;                            //!< Current parsed request.
        std::unique_ptr<bhttp::request_parser<bhttp::string_body>> parser_; //!< HTTP request parser.
        std::shared_ptr<net::steady_timer> timer_;                          //!< Per-request timeout timer.

        static const std::regex XSS_PATTERN; //!< Regex for basic XSS detection.
        static const std::regex SQL_PATTERN; //!< Regex for basic SQL injection detection.
    };

} // namespace vix

#endif // VIX_SESSION_HPP