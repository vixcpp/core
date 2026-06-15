/**
 *
 * @file router_dispatch_test.cpp
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
#include <memory>
#include <string>
#include <utility>

#include <vix/async/core/io_context.hpp>
#include <vix/async/core/task.hpp>
#include <vix/http/IRequestHandler.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/RequestState.hpp>
#include <vix/http/Response.hpp>
#include <vix/http/Status.hpp>
#include <vix/json/json.hpp>
#include <vix/router/Router.hpp>

namespace
{
  using Request = vix::http::Request;
  using Response = vix::http::Response;
  using Router = vix::router::Router;
  using task_void = vix::async::core::task<void>;

  struct DispatchResult
  {
    bool handled{false};
    Response response{};
  };

  struct HandlerState
  {
    int calls{0};
    std::string last_method{};
    std::string last_target{};
    std::string last_path{};
  };

  class TextHandler final : public vix::http::IRequestHandler
  {
  public:
    TextHandler(
        std::shared_ptr<HandlerState> state,
        int status,
        std::string body)
        : state_(std::move(state)),
          status_(status),
          body_(std::move(body))
    {
    }

    vix::async::core::task<void> handle_request(
        const Request &req,
        Response &res) override
    {
      if (state_)
      {
        ++state_->calls;
        state_->last_method = req.method();
        state_->last_target = req.target();
        state_->last_path = req.path();
      }

      Response::text_response(res, body_, status_);

      co_return;
    }

  private:
    std::shared_ptr<HandlerState> state_;
    int status_;
    std::string body_;
  };

  class JsonHandler final : public vix::http::IRequestHandler
  {
  public:
    explicit JsonHandler(std::shared_ptr<HandlerState> state)
        : state_(std::move(state))
    {
    }

    vix::async::core::task<void> handle_request(
        const Request &req,
        Response &res) override
    {
      if (state_)
      {
        ++state_->calls;
        state_->last_method = req.method();
        state_->last_target = req.target();
        state_->last_path = req.path();
      }

      Response::json_response(
          res,
          vix::json::Json{
              {"ok", true},
              {"method", req.method()},
              {"path", req.path()},
              {"target", req.target()},
          },
          vix::http::OK);

      co_return;
    }

  private:
    std::shared_ptr<HandlerState> state_;
  };

  static std::shared_ptr<HandlerState> make_state()
  {
    return std::make_shared<HandlerState>();
  }

  static std::shared_ptr<vix::http::IRequestHandler> make_text_handler(
      std::shared_ptr<HandlerState> state,
      int status,
      std::string body)
  {
    return std::make_shared<TextHandler>(
        std::move(state),
        status,
        std::move(body));
  }

  static std::shared_ptr<vix::http::IRequestHandler> make_json_handler(
      std::shared_ptr<HandlerState> state)
  {
    return std::make_shared<JsonHandler>(std::move(state));
  }

  static Request make_request(
      std::string method,
      std::string target,
      Request::HeaderMap headers = Request::HeaderMap{})
  {
    return Request{
        std::move(method),
        std::move(target),
        std::move(headers),
        std::string{},
        Request::ParamMap{},
        std::make_shared<vix::http::RequestState>()};
  }

  static task_void dispatch_app(
      vix::async::core::io_context &ctx,
      Router &router,
      const Request &req,
      Response &res,
      bool &handled)
  {
    handled = co_await router.handle_request(req, res);

    ctx.stop();

    co_return;
  }

  static DispatchResult dispatch(
      Router &router,
      const Request &req)
  {
    vix::async::core::io_context ctx;

    DispatchResult result;

    auto t = dispatch_app(
        ctx,
        router,
        req,
        result.response,
        result.handled);

    std::move(t).start(ctx.get_scheduler());

    ctx.run();

    return result;
  }

  static bool contains(const std::string &text, const std::string &needle)
  {
    return text.find(needle) != std::string::npos;
  }

  static vix::json::Json parse_body(const Response &res)
  {
    return vix::json::loads(res.body());
  }

  static void assert_common_final_headers(const Response &res)
  {
    assert(res.has_header("Server"));
    assert(res.header("Server") == "Vix.cpp");

    assert(res.has_header("Date"));
    assert(res.has_header("Content-Length"));
    assert(res.has_header("Connection"));
  }

  static void test_dispatch_static_get_route()
  {
    Router router;

    auto state = make_state();

    router.add_route(
        "GET",
        "/health",
        make_text_handler(state, vix::http::OK, "healthy"));

    const auto result = dispatch(
        router,
        make_request("GET", "/health"));

    assert(result.handled);
    assert(result.response.status() == vix::http::OK);
    assert(result.response.body() == "healthy");
    assert(result.response.header("Content-Type") == "text/plain; charset=utf-8");
    assert(result.response.header("Connection") == "keep-alive");
    assert(result.response.should_close() == false);
    assert_common_final_headers(result.response);

    assert(state->calls == 1);
    assert(state->last_method == "GET");
    assert(state->last_target == "/health");
    assert(state->last_path == "/health");
  }

  static void test_dispatch_normalizes_request_method()
  {
    Router router;

    auto state = make_state();

    router.add_route(
        "GET",
        "/users",
        make_text_handler(state, vix::http::OK, "users"));

    const auto result = dispatch(
        router,
        make_request("get", "/users"));

    assert(result.handled);
    assert(result.response.status() == vix::http::OK);
    assert(result.response.body() == "users");

    assert(state->calls == 1);
    assert(state->last_method == "get");
    assert(state->last_path == "/users");
  }

  static void test_dispatch_strips_query_for_matching()
  {
    Router router;

    auto state = make_state();

    router.add_route(
        "GET",
        "/users",
        make_text_handler(state, vix::http::OK, "users"));

    const auto result = dispatch(
        router,
        make_request("GET", "/users?page=2&limit=10"));

    assert(result.handled);
    assert(result.response.status() == vix::http::OK);
    assert(result.response.body() == "users");

    assert(state->calls == 1);
    assert(state->last_target == "/users?page=2&limit=10");
    assert(state->last_path == "/users");
  }

  static void test_dispatch_param_route()
  {
    Router router;

    auto state = make_state();

    router.add_route(
        "GET",
        "/users/{id}",
        make_text_handler(state, vix::http::OK, "user"));

    const auto result = dispatch(
        router,
        make_request("GET", "/users/42"));

    assert(result.handled);
    assert(result.response.status() == vix::http::OK);
    assert(result.response.body() == "user");

    assert(state->calls == 1);
    assert(state->last_target == "/users/42");
    assert(state->last_path == "/users/42");
  }

  static void test_dispatch_nested_param_route()
  {
    Router router;

    auto state = make_state();

    router.add_route(
        "GET",
        "/posts/{post_id}/comments/{comment_id}",
        make_text_handler(state, vix::http::OK, "comment"));

    const auto result = dispatch(
        router,
        make_request("GET", "/posts/10/comments/99"));

    assert(result.handled);
    assert(result.response.status() == vix::http::OK);
    assert(result.response.body() == "comment");

    assert(state->calls == 1);
    assert(state->last_target == "/posts/10/comments/99");
    assert(state->last_path == "/posts/10/comments/99");
  }

  static void test_dispatch_static_route_wins_over_param_route()
  {
    Router router;

    auto param_state = make_state();
    auto static_state = make_state();

    router.add_route(
        "GET",
        "/users/{id}",
        make_text_handler(param_state, vix::http::OK, "param"));

    router.add_route(
        "GET",
        "/users/me",
        make_text_handler(static_state, vix::http::OK, "static"));

    {
      const auto result = dispatch(
          router,
          make_request("GET", "/users/me"));

      assert(result.handled);
      assert(result.response.status() == vix::http::OK);
      assert(result.response.body() == "static");

      assert(static_state->calls == 1);
      assert(param_state->calls == 0);
    }

    {
      const auto result = dispatch(
          router,
          make_request("GET", "/users/42"));

      assert(result.handled);
      assert(result.response.status() == vix::http::OK);
      assert(result.response.body() == "param");

      assert(static_state->calls == 1);
      assert(param_state->calls == 1);
    }
  }

  static void test_dispatch_distinguishes_methods()
  {
    Router router;

    auto get_state = make_state();
    auto post_state = make_state();

    router.add_route(
        "GET",
        "/users",
        make_text_handler(get_state, vix::http::OK, "get-users"));

    router.add_route(
        "POST",
        "/users",
        make_text_handler(post_state, vix::http::CREATED, "created-user"));

    {
      const auto result = dispatch(
          router,
          make_request("GET", "/users"));

      assert(result.handled);
      assert(result.response.status() == vix::http::OK);
      assert(result.response.body() == "get-users");
    }

    {
      const auto result = dispatch(
          router,
          make_request("POST", "/users"));

      assert(result.handled);
      assert(result.response.status() == vix::http::CREATED);
      assert(result.response.body() == "created-user");
    }

    assert(get_state->calls == 1);
    assert(post_state->calls == 1);
  }

  static void test_dispatch_json_response()
  {
    Router router;

    auto state = make_state();

    router.add_route(
        "GET",
        "/api/status",
        make_json_handler(state));

    const auto result = dispatch(
        router,
        make_request("GET", "/api/status?debug=1"));

    assert(result.handled);
    assert(result.response.status() == vix::http::OK);
    assert(result.response.header("Content-Type") == "application/json; charset=utf-8");
    assert_common_final_headers(result.response);

    const auto body = parse_body(result.response);

    assert(body["ok"].get<bool>() == true);
    assert(body["method"].get<std::string>() == "GET");
    assert(body["path"].get<std::string>() == "/api/status");
    assert(body["target"].get<std::string>() == "/api/status?debug=1");

    assert(state->calls == 1);
  }

  static void test_dispatch_not_found_default_json_response()
  {
    Router router;

    const auto result = dispatch(
        router,
        make_request("GET", "/missing"));

    assert(result.handled);
    assert(result.response.status() == vix::http::NOT_FOUND);
    assert(result.response.header("Content-Type") == "application/json; charset=utf-8");
    assert(result.response.header("Connection") == "close");
    assert(result.response.should_close());
    assert_common_final_headers(result.response);

    const auto body = parse_body(result.response);

    assert(body["error"].get<std::string>() == "Route not found");
    assert(body["method"].get<std::string>() == "GET");
    assert(body["path"].get<std::string>() == "/missing");
  }

  static void test_dispatch_not_found_default_keeps_original_target_in_body()
  {
    Router router;

    const auto result = dispatch(
        router,
        make_request("POST", "/missing?debug=1"));

    assert(result.handled);
    assert(result.response.status() == vix::http::NOT_FOUND);

    const auto body = parse_body(result.response);

    assert(body["error"].get<std::string>() == "Route not found");
    assert(body["method"].get<std::string>() == "POST");
    assert(body["path"].get<std::string>() == "/missing?debug=1");
  }

  static void test_dispatch_custom_not_found_handler()
  {
    Router router;

    int calls = 0;

    router.setNotFoundHandler(
        [&calls](const Request &req, Response &res) -> vix::async::core::task<void>
        {
          ++calls;

          Response::json_response(
              res,
              vix::json::Json{
                  {"custom", true},
                  {"method", req.method()},
                  {"path", req.path()},
              },
              vix::http::NOT_FOUND);

          co_return;
        });

    const auto result = dispatch(
        router,
        make_request("DELETE", "/missing"));

    assert(result.handled);
    assert(result.response.status() == vix::http::NOT_FOUND);
    assert(result.response.header("Connection") == "keep-alive");
    assert(result.response.should_close() == false);
    assert_common_final_headers(result.response);

    const auto body = parse_body(result.response);

    assert(body["custom"].get<bool>() == true);
    assert(body["method"].get<std::string>() == "DELETE");
    assert(body["path"].get<std::string>() == "/missing");

    assert(calls == 1);
  }

  static void test_dispatch_head_falls_back_to_get_and_clears_body()
  {
    Router router;

    auto state = make_state();

    router.add_route(
        "GET",
        "/resource",
        make_text_handler(state, vix::http::OK, "resource-body"));

    const auto result = dispatch(
        router,
        make_request("HEAD", "/resource"));

    assert(result.handled);
    assert(result.response.status() == vix::http::OK);
    assert(result.response.body().empty());
    assert(result.response.header("Content-Length") == "13");
    assert(result.response.header("Connection") == "keep-alive");
    assert_common_final_headers(result.response);

    assert(state->calls == 1);
    assert(state->last_method == "HEAD");
    assert(state->last_target == "/resource");
  }

  static void test_dispatch_head_not_found_has_empty_body()
  {
    Router router;

    const auto result = dispatch(
        router,
        make_request("HEAD", "/missing"));

    assert(result.handled);
    assert(result.response.status() == vix::http::NOT_FOUND);
    assert(result.response.body().empty());

    assert(result.response.header("Content-Length") != "0");
    assert(result.response.header("Connection") == "close");
    assert(result.response.should_close());
    assert_common_final_headers(result.response);
  }

  static void test_dispatch_options_auto_no_content_when_options_route_missing()
  {
    Router router;

    router.add_route(
        "GET",
        "/users",
        make_text_handler(make_state(), vix::http::OK, "users"));

    const auto result = dispatch(
        router,
        make_request("OPTIONS", "/users"));

    assert(result.handled);
    assert(result.response.status() == vix::http::NO_CONTENT);
    assert(result.response.body().empty());
    assert(result.response.header("Connection") == "close");
    assert(result.response.should_close());
    assert(result.response.header("Content-Length") == "0");
    assert_common_final_headers(result.response);
  }

  static void test_dispatch_options_registered_route_wins()
  {
    Router router;

    auto state = make_state();

    router.add_route(
        "OPTIONS",
        "/users",
        make_text_handler(state, vix::http::OK, "options-users"));

    const auto result = dispatch(
        router,
        make_request("OPTIONS", "/users"));

    assert(result.handled);
    assert(result.response.status() == vix::http::OK);
    assert(result.response.body() == "options-users");
    assert(result.response.header("Connection") == "keep-alive");
    assert_common_final_headers(result.response);

    assert(state->calls == 1);
  }

  static void test_dispatch_no_content_from_handler_clears_body()
  {
    Router router;

    auto state = make_state();

    router.add_route(
        "DELETE",
        "/users/42",
        make_text_handler(state, vix::http::NO_CONTENT, "must be cleared"));

    const auto result = dispatch(
        router,
        make_request("DELETE", "/users/42"));

    assert(result.handled);
    assert(result.response.status() == vix::http::NO_CONTENT);
    assert(result.response.body().empty());
    assert(result.response.header("Content-Length") == "0");
    assert_common_final_headers(result.response);

    assert(state->calls == 1);
  }

  static void test_dispatch_not_modified_from_handler_clears_body()
  {
    Router router;

    auto state = make_state();

    router.add_route(
        "GET",
        "/cached",
        make_text_handler(state, vix::http::NOT_MODIFIED, "must be cleared"));

    const auto result = dispatch(
        router,
        make_request("GET", "/cached"));

    assert(result.handled);
    assert(result.response.status() == vix::http::NOT_MODIFIED);
    assert(result.response.body().empty());
    assert(result.response.header("Content-Length") == "0");
    assert_common_final_headers(result.response);

    assert(state->calls == 1);
  }

  static void test_dispatch_respects_request_connection_close()
  {
    Router router;

    router.add_route(
        "GET",
        "/close",
        make_text_handler(make_state(), vix::http::OK, "close"));

    const auto result = dispatch(
        router,
        make_request(
            "GET",
            "/close",
            Request::HeaderMap{
                {"Connection", "close"},
            }));

    assert(result.handled);
    assert(result.response.status() == vix::http::OK);
    assert(result.response.body() == "close");
    assert(result.response.header("Connection") == "close");
    assert(result.response.should_close());
    assert_common_final_headers(result.response);
  }

  static void test_dispatch_respects_request_connection_keep_alive()
  {
    Router router;

    router.add_route(
        "GET",
        "/keep",
        make_text_handler(make_state(), vix::http::OK, "keep"));

    const auto result = dispatch(
        router,
        make_request(
            "GET",
            "/keep",
            Request::HeaderMap{
                {"Connection", "keep-alive"},
            }));

    assert(result.handled);
    assert(result.response.status() == vix::http::OK);
    assert(result.response.body() == "keep");
    assert(result.response.header("Connection") == "keep-alive");
    assert(result.response.should_close() == false);
    assert_common_final_headers(result.response);
  }

  static void test_dispatch_does_not_override_handler_connection_header()
  {
    class ConnectionHandler final : public vix::http::IRequestHandler
    {
    public:
      vix::async::core::task<void> handle_request(
          const Request &,
          Response &res) override
      {
        Response::text_response(res, "custom connection", vix::http::OK);
        res.set_header("Connection", "upgrade");
        res.set_should_close(false);

        co_return;
      }
    };

    Router router;

    router.add_route(
        "GET",
        "/connection",
        std::make_shared<ConnectionHandler>());

    const auto result = dispatch(
        router,
        make_request("GET", "/connection"));

    assert(result.handled);
    assert(result.response.status() == vix::http::OK);
    assert(result.response.body() == "custom connection");
    assert(result.response.header("Connection") == "upgrade");
    assert(result.response.should_close() == false);
    assert_common_final_headers(result.response);
  }

} // namespace

int main()
{
  test_dispatch_static_get_route();
  test_dispatch_normalizes_request_method();
  test_dispatch_strips_query_for_matching();
  test_dispatch_param_route();
  test_dispatch_nested_param_route();
  test_dispatch_static_route_wins_over_param_route();
  test_dispatch_distinguishes_methods();
  test_dispatch_json_response();

  test_dispatch_not_found_default_json_response();
  test_dispatch_not_found_default_keeps_original_target_in_body();
  test_dispatch_custom_not_found_handler();

  test_dispatch_head_falls_back_to_get_and_clears_body();
  test_dispatch_head_not_found_has_empty_body();

  test_dispatch_options_auto_no_content_when_options_route_missing();
  test_dispatch_options_registered_route_wins();

  test_dispatch_no_content_from_handler_clears_body();
  test_dispatch_not_modified_from_handler_clears_body();

  test_dispatch_respects_request_connection_close();
  test_dispatch_respects_request_connection_keep_alive();
  test_dispatch_does_not_override_handler_connection_header();

  return 0;
}
