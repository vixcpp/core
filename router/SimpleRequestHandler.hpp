#ifndef VIX_SIMPLE_REQUEST_HANDLER_HPP
#define VIX_SIMPLE_REQUEST_HANDLER_HPP

#include <algorithm>
#include "IRequestHandler.hpp"

namespace Vix
{
    /**
     * @brief A simple HTTP request handler implementation.
     *
     * This class allows using a custom function to handle HTTP requests
     * by encapsulating the processing logic within a `std::function`.
     */
    class SimpleRequestHandler : public IRequestHandler
    {
    public:
        /**
         * @brief Constructor that takes a custom request handler function.
         *
         * @param handler Function to be executed when handling a request.
         */
        explicit SimpleRequestHandler(
            std::function<void(const http::request<http::string_body> &,
                               http::response<http::string_body> &)>
                handler);

        ~SimpleRequestHandler() {}

        /**
         * @brief Handles an HTTP request by invoking the provided handler function.
         *
         * @param req Incoming HTTP request.
         * @param res HTTP response to be populated and sent.
         */
        void handle_request(const http::request<http::string_body> &req,
                            http::response<http::string_body> &res) override;

    private:
        std::function<void(const http::request<http::string_body> &,
                           http::response<http::string_body> &)>
            handler_;
    };
}

#endif // VIX_SIMPLE_REQUEST_HANDLER_HPP
