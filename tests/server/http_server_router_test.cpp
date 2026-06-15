/**
 *
 * @file http_server_router_test.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <cassert>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>

#include <vix/config/Config.hpp>
#include <vix/executor/RuntimeExecutor.hpp>
#include <vix/http/IRequestHandler.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/RequestState.hpp>
#include <vix/http/Response.hpp>
#include <vix/router/RouteDoc.hpp>
#include <vix/router/RouteOptions.hpp>
#include <vix/router/Router.hpp>
#include <vix/server/HTTPServer.hpp>

namespace
{
  using Config = vix::config::Config;
  using HTTPServer = vix::server::HTTPServer;
  using RuntimeExecutor = vix::executor::RuntimeExecutor;
  using Router = vix::router::Router;
  using Request = vix::http::Request;

  class DummyHandler final : public vix::http::IRequestHandler
  {
  public:
    vix::async::core::task<void> handle_request(
        const vix::http::Request &,
        vix::http::Response &) override
    {
      co_return;
    }
  };

  static void set_env_var(const char *name, const std::string &value)
  {
#if defined(_WIN32)
    const std::string assignment = std::string{name} + "=" + value;
    const int rc = _putenv(assignment.c_str());
    assert(rc == 0);
#else
    const int rc = setenv(name, value.c_str(), 1);
    assert(rc == 0);
#endif
  }

  static Config make_config()
  {
    set_env_var("VIX_ENV_SILENT", "true");
    set_env_var("SERVER_PORT", "18100");
    set_env_var("SERVER_IO_THREADS", "1");
    set_env_var("SERVER_SESSION_TIMEOUT_SEC", "30");
    set_env_var("SERVER_TLS_ENABLED", "false");
    set_env_var("LOGGING_ASYNC", "false");

    Config config{};

    assert(config.getServerPort() == 18100);
    assert(config.getIOThreads() == 1);
    assert(config.isTlsEnabled() == false);

    return config;
  }

  static std::shared_ptr<RuntimeExecutor> make_executor()
  {
    return std::make_shared<RuntimeExecutor>(1);
  }

  static std::shared_ptr<vix::http::IRequestHandler> make_handler()
  {
    return std::make_shared<DummyHandler>();
  }

  static Request make_request(
      std::string method,
      std::string target)
  {
    return Request{
        std::move(method),
        std::move(target),
        Request::HeaderMap{},
        std::string{},
        Request::ParamMap{},
        std::make_shared<vix::http::RequestState>()};
  }

  static void test_server_creates_router()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    auto router = server.getRouter();

    assert(router != nullptr);
    assert(router->routes().empty());

    executor->stop();
  }

  static void test_get_router_returns_same_shared_router_each_time()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    auto first = server.getRouter();
    auto second = server.getRouter();

    assert(first != nullptr);
    assert(second != nullptr);
    assert(first == second);
    assert(first.get() == second.get());

    executor->stop();
  }

  static void test_routes_can_be_registered_through_server_router()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    auto router = server.getRouter();

    router->add_route(
        "GET",
        "/health",
        make_handler());

    assert(router->has_route("GET", "/health"));
    assert(router->has_route("get", "/health"));
    assert(router->has_route("GET", "health"));
    assert(router->has_route("GET", "/health?debug=1"));

    assert(!router->has_route("POST", "/health"));
    assert(!router->has_route("GET", "/missing"));

    assert(router->routes().size() == 1);
    assert(router->routes()[0].method == "GET");
    assert(router->routes()[0].path == "/health");
    assert(router->routes()[0].heavy == false);

    executor->stop();
  }

  static void test_multiple_routes_can_be_registered_through_server_router()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    auto router = server.getRouter();

    router->add_route("GET", "/users", make_handler());
    router->add_route("POST", "/users", make_handler());
    router->add_route("GET", "/users/{id}", make_handler());
    router->add_route("DELETE", "/users/{id}", make_handler());

    assert(router->has_route("GET", "/users"));
    assert(router->has_route("POST", "/users"));
    assert(router->has_route("GET", "/users/42"));
    assert(router->has_route("DELETE", "/users/42"));

    assert(!router->has_route("PUT", "/users"));
    assert(!router->has_route("PATCH", "/users/42"));

    assert(router->routes().size() == 4);

    assert(router->routes()[0].method == "GET");
    assert(router->routes()[0].path == "/users");

    assert(router->routes()[1].method == "POST");
    assert(router->routes()[1].path == "/users");

    assert(router->routes()[2].method == "GET");
    assert(router->routes()[2].path == "/users/{id}");

    assert(router->routes()[3].method == "DELETE");
    assert(router->routes()[3].path == "/users/{id}");

    executor->stop();
  }

  static void test_server_router_supports_param_routes()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    auto router = server.getRouter();

    router->add_route(
        "GET",
        "/projects/{project}/issues/{issue}",
        make_handler());

    assert(router->has_route("GET", "/projects/vix/issues/10"));
    assert(router->has_route("GET", "/projects/core/issues/99"));
    assert(router->has_route("GET", "/projects/core/issues/99?view=full"));

    assert(!router->has_route("GET", "/projects/core/issues"));
    assert(!router->has_route("GET", "/projects/core/issues/99/edit"));

    executor->stop();
  }

  static void test_server_router_static_route_wins_over_param_route()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    auto router = server.getRouter();

    router->add_route(
        "GET",
        "/users/{id}",
        make_handler(),
        vix::router::RouteOptions{.heavy = true});

    router->add_route(
        "GET",
        "/users/me",
        make_handler(),
        vix::router::RouteOptions{.heavy = false});

    Request param_req = make_request("GET", "/users/42");
    Request static_req = make_request("GET", "/users/me");

    assert(router->has_route("GET", "/users/42"));
    assert(router->has_route("GET", "/users/me"));

    assert(router->is_heavy(param_req) == true);
    assert(router->is_heavy(static_req) == false);

    executor->stop();
  }

  static void test_server_router_supports_heavy_routes()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    auto router = server.getRouter();

    router->add_route(
        "POST",
        "/reports/{id}/generate",
        make_handler(),
        vix::router::RouteOptions{.heavy = true});

    assert(router->has_route("POST", "/reports/42/generate"));
    assert(router->routes().size() == 1);
    assert(router->routes()[0].heavy == true);
    assert(router->routes()[0].doc.x["x-vix-heavy"].get<bool>() == true);

    Request req = make_request("POST", "/reports/42/generate?format=pdf");

    assert(router->is_heavy(req) == true);

    executor->stop();
  }

  static void test_server_router_supports_route_documentation()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    auto router = server.getRouter();

    vix::router::RouteDoc doc;

    doc.summary = "List users";
    doc.description = "Return registered users.";
    doc.tags = {"users", "read"};
    doc.responses = {
        {"200",
         {
             {"description", "Users returned"},
         }},
    };

    router->add_route(
        "GET",
        "/users",
        make_handler(),
        vix::router::RouteOptions{},
        doc);

    assert(router->has_route("GET", "/users"));
    assert(router->routes().size() == 1);

    const auto &record = router->routes()[0];

    assert(record.method == "GET");
    assert(record.path == "/users");
    assert(record.heavy == false);

    assert(record.doc.summary == "List users");
    assert(record.doc.description == "Return registered users.");
    assert(record.doc.tags.size() == 2);
    assert(record.doc.tags[0] == "users");
    assert(record.doc.tags[1] == "read");
    assert(record.doc.responses["200"]["description"].get<std::string>() == "Users returned");

    executor->stop();
  }

  static void test_server_router_default_not_found_handler_is_configured()
  {
    Config config = make_config();
    auto executor = make_executor();

    HTTPServer server{
        config,
        executor};

    auto router = server.getRouter();

    assert(router != nullptr);
    assert(!router->has_route("GET", "/missing"));

    /*
     * We do not execute the notFound handler here because that belongs to
     * router dispatch/session tests. This test only validates that the server
     * owns a usable router with the constructor-installed routing state.
     */

    executor->stop();
  }

  static void test_two_servers_have_independent_routers()
  {
    Config first_config = make_config();
    auto first_executor = make_executor();

    HTTPServer first{
        first_config,
        first_executor};

    Config second_config = make_config();
    auto second_executor = make_executor();

    HTTPServer second{
        second_config,
        second_executor};

    auto first_router = first.getRouter();
    auto second_router = second.getRouter();

    assert(first_router != nullptr);
    assert(second_router != nullptr);
    assert(first_router != second_router);

    first_router->add_route(
        "GET",
        "/only-first",
        make_handler());

    second_router->add_route(
        "GET",
        "/only-second",
        make_handler());

    assert(first_router->has_route("GET", "/only-first"));
    assert(!first_router->has_route("GET", "/only-second"));

    assert(second_router->has_route("GET", "/only-second"));
    assert(!second_router->has_route("GET", "/only-first"));

    first_executor->stop();
    second_executor->stop();
  }

} // namespace

int main()
{
  test_server_creates_router();
  test_get_router_returns_same_shared_router_each_time();

  test_routes_can_be_registered_through_server_router();
  test_multiple_routes_can_be_registered_through_server_router();
  test_server_router_supports_param_routes();
  test_server_router_static_route_wins_over_param_route();

  test_server_router_supports_heavy_routes();
  test_server_router_supports_route_documentation();

  test_server_router_default_not_found_handler_is_configured();
  test_two_servers_have_independent_routers();

  return 0;
}
