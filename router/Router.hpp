#ifndef VIX_ROUTER_HPP
#define VIX_ROUTER_HPP

#include "IRequestHandler.hpp"
#include "../http/Response.hpp"
#include "DynamicRequestHandler.hpp"

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <nlohmann/json.hpp>

#include <boost/regex.hpp>
#include <string>

#include <unordered_map>
#include <iostream>
#include <memory>
#include <string>
#include <spdlog/spdlog.h>
#include <regex>

namespace Vix
{

    namespace beast = boost::beast;
    namespace http = boost::beast::http;
    namespace net = boost::asio;

    using tcp = net::ip::tcp;
    using ssl_socket = boost::asio::ssl::stream<tcp::socket>;
    using json = nlohmann::json;

    /**
     * @brief Structure used to generate a unique hash for a key-value pair.
     *
     * This structure is designed to efficiently index routes in a hash table.
     * It combines the hashed values of both keys to create a unique hash,
     * enabling fast storage and retrieval of route-associated handlers.
     */
    struct PairHash
    {
        /**
         * @brief Generates a hash for a pair of keys.
         *
         * This function combines the hashed values of the two elements in the pair
         * to produce a single hash value suitable for use as a key in containers
         * like `std::unordered_map`.
         *
         * @tparam T1 The type of the first key in the pair.
         * @tparam T2 The type of the second key in the pair.
         * @param p The key pair to hash.
         * @return A combined hash value for the pair.
         */
        template <typename T1, typename T2>
        std::size_t operator()(const std::pair<T1, T2> &p) const
        {
            auto h1 = std::hash<T1>{}(p.first);
            auto h2 = std::hash<T2>{}(p.second);
            return h1 ^ (h2 << 1); // Simple way to combine two hashes
        }
    };

    /**
     * @brief Class representing a router for handling HTTP request routing.
     *
     * The router manages the dispatching of incoming HTTP requests to the appropriate
     * request handler based on the HTTP method (GET, POST, etc.) and the URL path.
     * It also supports dynamic routes and validates parameters within requests.
     */
    class Router
    {
    public:
        using RouteKey = std::pair<http::verb, std::string>;

        /**
         * @brief Default constructor to initialize the router.
         *
         * This constructor initializes an empty routing table, ready to accept new routes.
         */
        Router() : routes_(), route_patterns_() {}

        ~Router();

        /**
         * @brief Adds a new route to the router.
         *
         * This method associates a specific HTTP method and URL path with a request handler.
         * The handler will be invoked when a request matches the route.
         *
         * @param method The HTTP method for which this route should be active (e.g., GET, POST).
         * @param route The URL path associated with the route.
         * @param handler A request handler responsible for processing requests for this route.
         */
        void add_route(http::verb method, const std::string &route, std::shared_ptr<IRequestHandler> handler);

        /**
         * @brief Processes an incoming HTTP request.
         *
         * This method dispatches the request to the appropriate handler based on
         * the HTTP method and URL. It also generates the HTTP response.
         *
         * @param req The incoming HTTP request.
         * @param res The HTTP response to be populated.
         * @return Returns `true` if the request was successfully handled, `false` otherwise.
         */
        bool handle_request(const http::request<http::string_body> &req,
                            http::response<http::string_body> &res);

    private:
        /**
         * @brief Checks if a request path matches a dynamic route pattern.
         *
         * This method determines whether the request path matches a dynamically defined
         * route pattern (e.g., with parameters), and invokes the corresponding handler if matched.
         *
         * @param route_pattern The dynamic route pattern to test.
         * @param path The request path.
         * @param handler The request handler to invoke if matched.
         * @param res The HTTP response to populate.
         * @param req The HTTP request to process.
         * @return `true` if the dynamic route matches, `false` otherwise.
         */
        bool matches_dynamic_route(const std::string &route_pattern,
                                   const std::string &path,
                                   std::shared_ptr<IRequestHandler> handler,
                                   http::response<http::string_body> &res,
                                   const http::request<http::string_body> &req);

        /**
         * @brief Converts a route pattern to a regular expression.
         *
         * This method transforms a route pattern string into a regular expression,
         * enabling support for dynamic route matching.
         *
         * @param route_pattern The route pattern string.
         * @return A regular expression string representing the pattern.
         */
        static std::string convert_route_to_regex(const std::string &route_pattern);

        /**
         * @brief Sanitizes an input string to prevent security risks.
         *
         * This method escapes or removes potentially harmful characters from a string,
         * such as those that could affect URL parsing or system safety.
         *
         * @param input The input string to sanitize.
         * @return The sanitized string.
         */
        std::string sanitize_input(const std::string &input);

        /**
         * @brief Validates extracted parameters from the URL or request body.
         *
         * This method ensures that parameters meet the expected format or criteria
         * before proceeding with response generation.
         *
         * @param params The map of extracted parameters.
         * @param res The HTTP response to populate in case of validation failure.
         * @return `true` if parameters are valid, `false` otherwise.
         */
        bool validate_parameters(const std::unordered_map<std::string, std::string> &params,
                                 http::response<http::string_body> &res);

        /**
         * @brief The routing table indexed by (HTTP method, URL path) pairs.
         *
         * This table stores all registered routes. The key is a pair consisting of the HTTP method
         * and the URL path, and the value is the corresponding request handler.
         */
        std::unordered_map<RouteKey, std::shared_ptr<IRequestHandler>, PairHash> routes_;

        /**
         * @brief Converts a parameter map to a formatted string.
         *
         * This method transforms a parameter map into a string representation
         * for display or logging purposes.
         *
         * @param map The map of parameters.
         * @return A formatted string representing the parameters.
         */
        std::string map_to_string(const std::unordered_map<std::string, std::string> &map);

        std::vector<std::string> route_patterns_; ///< Stores all defined dynamic route patterns.
    };

};

#endif
