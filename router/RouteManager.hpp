#pragma once
#include "HTTPServer.hpp"
#include <functional>
#include <vector>

namespace Vix
{
    class RouteManager
    {
    public:
        using RouteHandler = std::function<void(Router &router)>;

        void add_route(RouteHandler handler)
        {
            route_handlers_.push_back(handler);
        }

        void setup_routes(Router &router)
        {
            for (auto &handler : route_handlers_)
            {
                handler(router);
            }
        }

    private:
        std::vector<RouteHandler> route_handlers_;
    };
}
