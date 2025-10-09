#ifndef I_REQUEST_HANDLER_HPP
#define I_REQUEST_HANDLER_HPP

#include <boost/beast/http.hpp>

namespace http = boost::beast::http;

namespace Vix
{
    /**
     * @file IRequestHandler.hpp
     * @brief Base interface for all HTTP route handlers in Vix.cpp.
     *
     * @details
     * This interface defines the **minimal contract** for any component that
     * wants to handle an HTTP request within the Vix routing system.
     *
     * All concrete handlers—custom user code or framework adapters—must implement
     * the `handle_request()` method, which receives a parsed request and an
     * outgoing response reference.
     *
     * Typically, developers do not implement this interface directly; instead,
     * they use the templated adapter `Vix::RequestHandler<Handler>` to wrap
     * lambdas or functors.
     *
     * ### Example
     * ```cpp
     * class MyHandler : public Vix::IRequestHandler {
     * public:
     *     void handle_request(const http::request<http::string_body>& req,
     *                         http::response<http::string_body>& res) override {
     *         res.result(http::status::ok);
     *         res.body() = "Hello from custom handler!";
     *         res.prepare_payload();
     *     }
     * };
     * ```
     *
     * Then register:
     * ```cpp
     * router.add_route(http::verb::get, "/hello", std::make_shared<MyHandler>());
     * ```
     */
    class IRequestHandler
    {
    public:
        /**
         * @brief Process an HTTP request and populate a response.
         *
         * @param req The fully parsed incoming HTTP request (read-only).
         * @param res The HTTP response to populate (body, headers, status).
         *
         * @note Implementations must always call `res.prepare_payload()` or use
         * the helpers in `Vix::Response` to ensure correct Content-Length and
         * header consistency.
         */
        virtual void handle_request(const http::request<http::string_body> &req,
                                    http::response<http::string_body> &res) = 0;

        /** @brief Default constructor. */
        IRequestHandler() noexcept = default;

        /** @brief Virtual destructor to allow polymorphic cleanup. */
        virtual ~IRequestHandler() noexcept = default;

        // --- Non-copyable and non-movable ---
        IRequestHandler(const IRequestHandler &) = delete;
        IRequestHandler &operator=(const IRequestHandler &) = delete;
        IRequestHandler(IRequestHandler &&) noexcept = delete;
        IRequestHandler &operator=(IRequestHandler &&) noexcept = delete;
    };

} // namespace Vix

#endif // I_REQUEST_HANDLER_HPP
