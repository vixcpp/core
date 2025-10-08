#ifndef I_REQUEST_HANDLER_HPP
#define I_REQUEST_HANDLER_HPP

#include <boost/beast/http.hpp>

namespace http = boost::beast::http;

namespace Vix
{
    /**
     * @brief Interface for HTTP route handlers.
     *
     * This interface defines the `handle_request` method, which must be implemented
     * by any class that wants to handle an HTTP request and generate a response.
     */
    class IRequestHandler
    {
    public:
        /**
         * @brief Handles an HTTP request and generates a response.
         *
         * @param req The incoming HTTP request.
         * @param res The HTTP response to send back to the client.
         */
        virtual void handle_request(const http::request<http::string_body> &req,
                                    http::response<http::string_body> &res) = 0;

        // --- Constructors / destructor ---
        IRequestHandler() noexcept = default;
        virtual ~IRequestHandler() noexcept override = default;

        // --- Disable copy & move semantics (interfaces are non-owning) ---
        IRequestHandler(const IRequestHandler &) = delete;
        IRequestHandler &operator=(const IRequestHandler &) = delete;
        IRequestHandler(IRequestHandler &&) noexcept = delete;
        IRequestHandler &operator=(IRequestHandler &&) noexcept = delete;
    };
} // namespace Vix

#endif // I_REQUEST_HANDLER_HPP
