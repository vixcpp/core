#ifndef VIX_DYNAMIC_REQUEST_HANDLER_HPP
#define VIX_DYNAMIC_REQUEST_HANDLER_HPP

#include "IRequestHandler.hpp"
#include <algorithm>
#include <string>
#include <unordered_map>
#include <functional>
#include <boost/beast/http.hpp>

namespace Vix
{
    /**
     * @brief Class for handling requests with dynamic URL parameters.
     *
     * This class allows processing routes that contain dynamic parameters,
     * such as /users/{id} or /products/{slug}, and passes those parameters
     * to a custom handler function.
     */
    class DynamicRequestHandler : public IRequestHandler
    {
    public:
        /**
         * @brief Constructor to initialize the dynamic request handler.
         *
         * @param handler The function that handles the request using the extracted dynamic parameters.
         */
        explicit DynamicRequestHandler(std::function<void(const std::unordered_map<std::string, std::string> &,
                                                          http::response<http::string_body> &)>
                                           handler);

        ~DynamicRequestHandler();

        /**
         * @brief Implements the handle_request method from IRequestHandler.
         *
         * @param req The incoming HTTP request (not used here, as parameters are already extracted).
         * @param res The HTTP response to be populated and sent.
         */
        void handle_request(const http::request<http::string_body> &req,
                            http::response<http::string_body> &res) override;

        /**
         * @brief Updates the dynamic parameters for this route.
         *
         * @param params The parameters extracted from the URL.
         * @param res The HTTP response object (may be modified by the handler).
         */
        void set_params(const std::unordered_map<std::string, std::string> &params,
                        http::response<http::string_body> &res);

    private:
        std::unordered_map<std::string, std::string> params_;
        std::function<void(const std::unordered_map<std::string, std::string> &,
                           http::response<http::string_body> &)>
            handler_;
    };

};

#endif
