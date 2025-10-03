#include "RouteConfigurator.hpp"
#include "../router/DynamicRequestHandler.hpp"
#include "../router/UnifiedRequestHandler.hpp"

namespace Vix
{
    class Controller
    {
    public:
        explicit Controller(Config &config) : config_(config) {}

        virtual ~Controller() = default;

        virtual void configure(Router &router) = 0;

    protected:
        Config &config_;

        template <typename Handler>
        void add_route(Router &router, http::verb method, const std::string &path, Handler handler)
        {
            router.add_route(
                method, path,
                std::static_pointer_cast<IRequestHandler>(
                    std::make_shared<UnifiedRequestHandler>(handler)));
        }
    };

    class TestController : public Controller
    {
    public:
        using Controller::Controller;

        void configure(Router &router) override
        {
            add_route(router, http::verb::get, "/",
                      [](const http::request<http::string_body> &,
                         http::response<http::string_body> &res)
                      {
                          Response::success_response(res, "Hello from the Vix framework");
                      });
        }
    };

    RouteConfigurator::RouteConfigurator(Router &router)
        : router_(router)
    {
    }

    void RouteConfigurator::configure_routes()
    {
        Config &config = Config::getInstance();
        config.loadConfig();

        std::unique_ptr<TestController> testController = std::make_unique<TestController>(config);
        testController->configure(router_);
    }
}
