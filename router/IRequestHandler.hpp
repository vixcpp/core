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
         * This method is called when the router matches a route to an HTTP request.
         * It is responsible for creating and populating the appropriate HTTP response.
         *
         * @param req The incoming HTTP request.
         * @param res The HTTP response to send back to the client.
         */
        virtual void handle_request(const http::request<http::string_body> &req,
                                    http::response<http::string_body> &res) = 0;

        IRequestHandler() = default;

        // Virtual destructor to ensure proper cleanup of derived objects.
        virtual ~IRequestHandler() = default;

        // Disable copy constructor and assignment operator.
        IRequestHandler(const IRequestHandler &) = delete;
        void operator=(const IRequestHandler &) = delete;
    };
}

#endif
