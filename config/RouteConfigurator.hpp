#ifndef VIX_ROUTE_CONFIGURATOR_HPP
#define VIX_ROUTE_CONFIGURATOR_HPP

#include "../router/Router.hpp"
#include "Config.hpp"
#include <unordered_map>
#include <string>
#include <memory>

namespace Vix
{
    /**
     * @brief The RouteConfigurator class is responsible for configuring routes in the router.
     *
     * It handles the registration of both static and dynamic routes
     * for the HTTP server using the provided router.
     */
    class RouteConfigurator
    {
    public:
        /**
         * @brief Constructor for the RouteConfigurator class.
         *
         * @param router The router instance to be configured with routes.
         */
        explicit RouteConfigurator(Router &router);

        /**
         * @brief Configures the routes for the HTTP server.
         *
         * This method registers static and dynamic routes in the router,
         * setting up request handlers for various server endpoints,
         * such as "/users/{id}" or "/products/{slug}".
         */
        void configure_routes();

    private:
        Router &router_; /**< The router used to add and manage routes. */
    };

}

#endif
