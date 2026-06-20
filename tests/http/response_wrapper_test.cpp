/**
 *
 * @file response_wrapper_test.cpp
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
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>

#include <vix/http/ResponseWrapper.hpp>
#include <vix/http/Status.hpp>
#include <vix/json/Simple.hpp>
#include <vix/json/json.hpp>
#include <vix/template/Engine.hpp>
#include <vix/template/StringLoader.hpp>
#include <vix/ui/core/View.hpp>
#include <vix/ui/html/HtmlResponse.hpp>
#include <vix/view/TemplateView.hpp>

namespace
{
  using Response = vix::http::Response;
  using ResponseWrapper = vix::http::ResponseWrapper;

  static bool contains(const std::string &text, const std::string &needle)
  {
    return text.find(needle) != std::string::npos;
  }

  static vix::json::Json parse_body(const Response &res)
  {
    return vix::json::loads(res.body());
  }

  static std::filesystem::path temp_test_dir()
  {
    auto dir = std::filesystem::temp_directory_path() / "vix_response_wrapper_test";
    std::filesystem::create_directories(dir);
    return dir;
  }

  static std::shared_ptr<vix::view::TemplateView> make_template_view()
  {
    auto loader = std::make_shared<vix::template_::StringLoader>();

    loader->set(
        "ui/home.html",
        "<h1>{{ page_title }}</h1><p>Hello {{ name }}</p>");

    auto engine = std::make_shared<vix::template_::Engine>(loader);

    return std::make_shared<vix::view::TemplateView>(engine);
  }

  static void write_file(const std::filesystem::path &path, const std::string &body)
  {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    assert(out);
    out << body;
  }

  static void test_wrapper_initializes_invalid_status_to_ok()
  {
    Response res{0};

    ResponseWrapper out{res};

    assert(res.status() == vix::http::OK);
    assert(!out.has_body());
  }

  static void test_status_and_status_alias()
  {
    Response res;
    ResponseWrapper out{res};

    out.status(vix::http::CREATED);

    assert(res.status() == vix::http::CREATED);

    out.set_status(vix::http::ACCEPTED);

    assert(res.status() == vix::http::ACCEPTED);
  }

  static void test_compile_time_status_helpers()
  {
    Response res;
    ResponseWrapper out{res};

    out.status_c<vix::http::CREATED>();

    assert(res.status() == vix::http::CREATED);

    out.set_status_c<vix::http::NO_CONTENT>();

    assert(res.status() == vix::http::NO_CONTENT);
  }

  static void test_header_set_append_and_type()
  {
    Response res;
    ResponseWrapper out{res};

    out.header("X-Test", "yes")
        .set("X-Set", "value")
        .append("Cache-Control", "no-store")
        .append("Cache-Control", "private")
        .type("text/custom");

    assert(res.header("X-Test") == "yes");
    assert(res.header("X-Set") == "value");
    assert(res.header("Cache-Control") == "no-store, private");
    assert(res.header("Content-Type") == "text/custom");

    out.contentType("application/custom");

    assert(res.header("Content-Type") == "application/custom");
    assert(out.has_header("Content-Type"));
  }

  static void test_text_response_sets_plain_text_defaults()
  {
    Response res;
    ResponseWrapper out{res};

    out.status(vix::http::CREATED).text("created");

    assert(res.status() == vix::http::CREATED);
    assert(res.body() == "created");
    assert(res.header("Content-Type") == "text/plain; charset=utf-8");
    assert(res.header("X-Content-Type-Options") == "nosniff");
    assert(res.header("Content-Length") == "7");
    assert(out.has_body());
  }

  static void test_text_response_preserves_existing_content_type()
  {
    Response res;
    ResponseWrapper out{res};

    out.type("text/custom").text("hello");

    assert(res.status() == vix::http::OK);
    assert(res.body() == "hello");
    assert(res.header("Content-Type") == "text/custom");
  }

  static void test_ui_html_response()
  {
    Response res;
    ResponseWrapper out{res};

    vix::ui::HtmlResponse html =
        vix::ui::HtmlResponse::html("<h1>Hello UI</h1>", vix::http::CREATED);

    out.ui(html);

    assert(res.status() == vix::http::CREATED);
    assert(res.body() == "<h1>Hello UI</h1>");
    assert(res.header("Content-Type") == "text/html; charset=utf-8");
    assert(res.header("X-Content-Type-Options") == "nosniff");
  }

  static void test_ui_html_response_no_content_clears_body()
  {
    Response res;
    ResponseWrapper out{res};

    vix::ui::HtmlResponse html =
        vix::ui::HtmlResponse::html("<h1>ignored</h1>", vix::http::NO_CONTENT);

    out.ui(html);

    assert(res.status() == vix::http::NO_CONTENT);
    assert(res.body().empty());
  }

  static void test_ui_view_response()
  {
    auto template_view = make_template_view();

    Response res;
    ResponseWrapper out{res, template_view.get()};

    vix::ui::View view("ui/home.html");
    view.set_title("Home");
    view.set("name", "Gaspard");

    out.ui(view);

    assert(res.status() == vix::http::OK);
    assert(res.header("Content-Type") == "text/html; charset=utf-8");
    assert(res.header("X-Content-Type-Options") == "nosniff");
    assert(res.body() == "<h1>Home</h1><p>Hello Gaspard</p>");
  }

  static void test_ui_view_preserves_existing_status()
  {
    auto template_view = make_template_view();

    Response res;
    ResponseWrapper out{res, template_view.get()};

    vix::ui::View view("ui/home.html");
    view.set_title("Missing");
    view.set("name", "Gaspard");

    out.status(vix::http::NOT_FOUND).ui(view);

    assert(res.status() == vix::http::NOT_FOUND);
    assert(res.header("Content-Type") == "text/html; charset=utf-8");
    assert(res.body() == "<h1>Missing</h1><p>Hello Gaspard</p>");
  }

  static void test_ui_view_requires_template_view()
  {
    Response res;
    ResponseWrapper out{res};

    vix::ui::View view("ui/home.html");
    view.set_title("Home");
    view.set("name", "Gaspard");

    bool thrown = false;

    try
    {
      out.ui(view);
    }
    catch (const std::runtime_error &)
    {
      thrown = true;
    }

    assert(thrown);
  }

  static void test_send_string_variants()
  {
    {
      Response res;
      ResponseWrapper out{res};

      out.send("hello");

      assert(res.status() == vix::http::OK);
      assert(res.body() == "hello");
      assert(res.header("Content-Type") == "text/plain; charset=utf-8");
    }

    {
      Response res;
      ResponseWrapper out{res};

      const char *value = nullptr;
      out.send(value);

      assert(res.status() == vix::http::OK);
      assert(res.body().empty());
      assert(res.header("Content-Type") == "text/plain; charset=utf-8");
    }

    {
      Response res;
      ResponseWrapper out{res};

      std::string value = "from std::string";
      out.send(value);

      assert(res.status() == vix::http::OK);
      assert(res.body() == "from std::string");
      assert(res.header("Content-Type") == "text/plain; charset=utf-8");
    }
  }

  static void test_send_without_payload_adds_default_status_body()
  {
    Response res;
    ResponseWrapper out{res};

    out.status(vix::http::NOT_FOUND).send();

    assert(res.status() == vix::http::NOT_FOUND);
    assert(res.body() == "404 Not Found");
    assert(res.header("Content-Type") == "text/plain; charset=utf-8");
  }

  static void test_send_without_payload_keeps_existing_body()
  {
    Response res;
    ResponseWrapper out{res};

    out.status(vix::http::OK).text("already set").send();

    assert(res.status() == vix::http::OK);
    assert(res.body() == "already set");
  }

  static void test_send_no_content_clears_body()
  {
    Response res;
    ResponseWrapper out{res};

    out.status(vix::http::NO_CONTENT).send("must be ignored");

    assert(res.status() == vix::http::NO_CONTENT);
    assert(res.body().empty());
  }

  static void test_send_not_modified_clears_body()
  {
    Response res;
    ResponseWrapper out{res};

    out.status(vix::http::NOT_MODIFIED).send("must be ignored");

    assert(res.status() == vix::http::NOT_MODIFIED);
    assert(res.body().empty());
  }

  static void test_send_status()
  {
    {
      Response res;
      ResponseWrapper out{res};

      out.sendStatus(vix::http::NOT_FOUND);

      assert(res.status() == vix::http::NOT_FOUND);
      assert(res.body() == "404 Not Found");
      assert(res.header("Content-Type") == "text/plain; charset=utf-8");
    }

    {
      Response res;
      ResponseWrapper out{res};

      out.sendStatus(vix::http::NO_CONTENT);

      assert(res.status() == vix::http::NO_CONTENT);
      assert(res.body().empty());
    }
  }

  static void test_send_status_and_payload()
  {
    Response res;
    ResponseWrapper out{res};

    out.send(vix::http::CREATED, "created");

    assert(res.status() == vix::http::CREATED);
    assert(res.body() == "created");
    assert(res.header("Content-Type") == "text/plain; charset=utf-8");
  }

  static void test_send_compile_time_status_and_payload()
  {
    Response res;
    ResponseWrapper out{res};

    out.send_c<vix::http::ACCEPTED>("accepted");

    assert(res.status() == vix::http::ACCEPTED);
    assert(res.body() == "accepted");
    assert(res.header("Content-Type") == "text/plain; charset=utf-8");
  }

  static void test_json_response_with_vix_json()
  {
    Response res;
    ResponseWrapper out{res};

    vix::json::Json payload{
        {"ok", true},
        {"runtime", "vix"},
        {"count", 3},
    };

    out.status(vix::http::ACCEPTED).json(payload);

    assert(res.status() == vix::http::ACCEPTED);
    assert(res.header("Content-Type") == "application/json; charset=utf-8");
    assert(res.header("X-Content-Type-Options") == "nosniff");

    const auto parsed = parse_body(res);

    assert(parsed["ok"].get<bool>() == true);
    assert(parsed["runtime"].get<std::string>() == "vix");
    assert(parsed["count"].get<int>() == 3);
  }

  static void test_send_json_alias()
  {
    Response res;
    ResponseWrapper out{res};

    out.send(vix::json::Json{
        {"name", "Vix.cpp"},
        {"module", "core"},
    });

    const auto parsed = parse_body(res);

    assert(res.status() == vix::http::OK);
    assert(res.header("Content-Type") == "application/json; charset=utf-8");
    assert(parsed["name"].get<std::string>() == "Vix.cpp");
    assert(parsed["module"].get<std::string>() == "core");
  }

  static void test_json_response_preserves_existing_content_type()
  {
    Response res;
    ResponseWrapper out{res};

    out.type("application/custom+json")
        .json(vix::json::Json{{"ok", true}});

    assert(res.header("Content-Type") == "application/custom+json");

    const auto parsed = parse_body(res);

    assert(parsed["ok"].get<bool>() == true);
  }

  static void test_ordered_json_response_helper()
  {
    Response res;

    vix::json::OrderedJson payload = vix::json::o(
        "first", 1,
        "second", 2,
        "third", 3);

    vix::http::ordered_json_response(res, payload, vix::http::CREATED);

    assert(res.status() == vix::http::CREATED);
    assert(res.header("Content-Type") == "application/json; charset=utf-8");

    const auto parsed = parse_body(res);

    assert(parsed["first"].get<int>() == 1);
    assert(parsed["second"].get<int>() == 2);
    assert(parsed["third"].get<int>() == 3);
  }

  static void test_json_ordered()
  {
    Response res;
    ResponseWrapper out{res};

    vix::json::OrderedJson payload = vix::json::o(
        "name", "Vix.cpp",
        "stable", true);

    out.status(vix::http::CREATED).json_ordered(payload);

    assert(res.status() == vix::http::CREATED);
    assert(res.header("Content-Type") == "application/json; charset=utf-8");

    const auto parsed = parse_body(res);

    assert(parsed["name"].get<std::string>() == "Vix.cpp");
    assert(parsed["stable"].get<bool>() == true);
  }

  static void test_token_to_json_primitives()
  {
    assert(vix::http::token_to_json(vix::json::token{}).is_null());
    assert(vix::http::token_to_json(vix::json::token{nullptr}).is_null());
    assert(vix::http::token_to_json(vix::json::token{true}).get<bool>() == true);
    assert(vix::http::token_to_json(vix::json::token{42}).get<int>() == 42);
    assert(vix::http::token_to_json(vix::json::token{3.5}).get<double>() == 3.5);
    assert(vix::http::token_to_json(vix::json::token{"vix"}).get<std::string>() == "vix");
  }

  static void test_array_to_json()
  {
    vix::json::array_t arr;

    arr.push_cstr("C++");
    arr.push_int(42);
    arr.push_bool(true);
    arr.push_null();

    const auto json = vix::http::array_to_json(arr);

    assert(json.is_array());
    assert(json.size() == 4);
    assert(json[0].get<std::string>() == "C++");
    assert(json[1].get<int>() == 42);
    assert(json[2].get<bool>() == true);
    assert(json[3].is_null());
  }

  static void test_kvs_to_json()
  {
    vix::json::kvs kv{
        "name",
        "Vix.cpp",
        "module",
        "core",
        "ok",
        true,
        "count",
        3,
    };

    const auto json = vix::http::kvs_to_json(kv);

    assert(json.is_object());
    assert(json["name"].get<std::string>() == "Vix.cpp");
    assert(json["module"].get<std::string>() == "core");
    assert(json["ok"].get<bool>() == true);
    assert(json["count"].get<int>() == 3);
  }

  static void test_kvs_to_json_ignores_non_string_keys()
  {
    vix::json::kvs kv{
        123,
        "ignored",
        "valid",
        "kept",
    };

    const auto json = vix::http::kvs_to_json(kv);

    assert(!json.contains("123"));
    assert(json["valid"].get<std::string>() == "kept");
  }

  static void test_json_response_with_simple_kvs()
  {
    Response res;
    ResponseWrapper out{res};

    vix::json::kvs payload{
        "source",
        "simple",
        "compatible",
        true,
        "count",
        7,
    };

    out.json(payload);

    assert(res.status() == vix::http::OK);
    assert(res.header("Content-Type") == "application/json; charset=utf-8");

    const auto parsed = parse_body(res);

    assert(parsed["source"].get<std::string>() == "simple");
    assert(parsed["compatible"].get<bool>() == true);
    assert(parsed["count"].get<int>() == 7);
  }

  static void test_json_response_with_simple_array()
  {
    Response res;
    ResponseWrapper out{res};

    vix::json::array_t payload;

    payload.push_cstr("http");
    payload.push_cstr("json");
    payload.push_int(2);

    out.json(payload);

    assert(res.status() == vix::http::OK);
    assert(res.header("Content-Type") == "application/json; charset=utf-8");

    const auto parsed = parse_body(res);

    assert(parsed.is_array());
    assert(parsed[0].get<std::string>() == "http");
    assert(parsed[1].get<std::string>() == "json");
    assert(parsed[2].get<int>() == 2);
  }

  static void test_json_response_with_simple_token_object()
  {
    Response res;
    ResponseWrapper out{res};

    vix::json::kvs object{
        "runtime",
        "vix",
        "ok",
        true,
    };

    vix::json::token payload{object};

    out.json(payload);

    assert(res.status() == vix::http::OK);

    const auto parsed = parse_body(res);

    assert(parsed["runtime"].get<std::string>() == "vix");
    assert(parsed["ok"].get<bool>() == true);
  }

  static void test_send_simple_aliases()
  {
    {
      Response res;
      ResponseWrapper out{res};

      vix::json::kvs payload{
          "kind",
          "kvs",
          "ok",
          true,
      };

      out.send(payload);

      const auto parsed = parse_body(res);

      assert(parsed["kind"].get<std::string>() == "kvs");
      assert(parsed["ok"].get<bool>() == true);
    }

    {
      Response res;
      ResponseWrapper out{res};

      vix::json::array_t payload;
      payload.push_int(1);
      payload.push_int(2);
      payload.push_int(3);

      out.send(payload);

      const auto parsed = parse_body(res);

      assert(parsed.is_array());
      assert(parsed[0].get<int>() == 1);
      assert(parsed[1].get<int>() == 2);
      assert(parsed[2].get<int>() == 3);
    }

    {
      Response res;
      ResponseWrapper out{res};

      vix::json::token payload{"token-string"};

      out.send(payload);

      const auto parsed = parse_body(res);

      assert(parsed.get<std::string>() == "token-string");
    }
  }

  static void test_initializer_list_json()
  {
    Response res;
    ResponseWrapper out{res};

    out.json({
        "name",
        "Vix.cpp",
        "module",
        "core",
        "ok",
        true,
    });

    const auto parsed = parse_body(res);

    assert(parsed["name"].get<std::string>() == "Vix.cpp");
    assert(parsed["module"].get<std::string>() == "core");
    assert(parsed["ok"].get<bool>() == true);
  }

  static void test_redirect_default_found()
  {
    Response res;
    ResponseWrapper out{res};

    out.redirect("/login");

    assert(res.status() == vix::http::FOUND);
    assert(res.header("Location") == "/login");
    assert(res.header("Content-Type") == "text/html; charset=utf-8");
    assert(res.header("X-Content-Type-Options") == "nosniff");
    assert(contains(res.body(), "Redirecting to /login"));
  }

  static void test_redirect_custom_status()
  {
    Response res;
    ResponseWrapper out{res};

    out.redirect(vix::http::MOVED_PERMANENTLY, "/new-path");

    assert(res.status() == vix::http::MOVED_PERMANENTLY);
    assert(res.header("Location") == "/new-path");
    assert(res.header("Content-Type") == "text/html; charset=utf-8");
    assert(contains(res.body(), "Redirecting to /new-path"));
  }

  static void test_location_header()
  {
    Response res;
    ResponseWrapper out{res};

    out.status(vix::http::TEMPORARY_REDIRECT)
        .location("/temporary")
        .send();

    assert(res.status() == vix::http::TEMPORARY_REDIRECT);
    assert(res.header("Location") == "/temporary");
    assert(res.body() == "307 Temporary Redirect");
  }

  static void test_default_status_message()
  {
    assert(ResponseWrapper::default_status_message(vix::http::OK) == "200 OK");
    assert(ResponseWrapper::default_status_message(vix::http::NOT_FOUND) == "404 Not Found");
    assert(ResponseWrapper::default_status_message(599) == "599 Unknown");
  }

  static void test_mime_from_ext()
  {
    assert(ResponseWrapper::mime_from_ext(".html") == "text/html; charset=utf-8");
    assert(ResponseWrapper::mime_from_ext(".css") == "text/css; charset=utf-8");
    assert(ResponseWrapper::mime_from_ext(".js") == "application/javascript; charset=utf-8");
    assert(ResponseWrapper::mime_from_ext(".json") == "application/json; charset=utf-8");
    assert(ResponseWrapper::mime_from_ext(".png") == "image/png");
    assert(ResponseWrapper::mime_from_ext(".jpg") == "image/jpeg");
    assert(ResponseWrapper::mime_from_ext(".jpeg") == "image/jpeg");
    assert(ResponseWrapper::mime_from_ext(".gif") == "image/gif");
    assert(ResponseWrapper::mime_from_ext(".svg") == "image/svg+xml");
    assert(ResponseWrapper::mime_from_ext(".ico") == "image/x-icon");
    assert(ResponseWrapper::mime_from_ext(".txt") == "text/plain; charset=utf-8");
    assert(ResponseWrapper::mime_from_ext(".woff") == "font/woff");
    assert(ResponseWrapper::mime_from_ext(".woff2") == "font/woff2");
    assert(ResponseWrapper::mime_from_ext(".unknown") == "application/octet-stream");
  }

  static void test_read_file_binary()
  {
    const auto dir = temp_test_dir();
    const auto path = dir / "binary.txt";

    write_file(path, "hello file");

    std::string out;

    assert(ResponseWrapper::read_file_binary(path, out));
    assert(out == "hello file");

    out.clear();

    assert(!ResponseWrapper::read_file_binary(dir / "missing.txt", out));
  }

  static void test_file_response_for_text_file()
  {
    const auto dir = temp_test_dir();
    const auto path = dir / "hello.txt";

    write_file(path, "hello file");

    Response res;
    ResponseWrapper out{res};

    out.file(path);

    assert(res.status() == vix::http::OK);
    assert(res.body() == "hello file");
    assert(res.header("Content-Type") == "text/plain; charset=utf-8");
    assert(res.header("X-Content-Type-Options") == "nosniff");
    assert(res.header("Cache-Control") == "public, max-age=3600");
  }

  static void test_file_response_for_missing_file()
  {
    const auto dir = temp_test_dir();

    Response res;
    ResponseWrapper out{res};

    out.file(dir / "missing.txt");

    assert(res.status() == vix::http::NOT_FOUND);
    assert(res.body() == "Not Found");
    assert(res.header("Content-Type") == "text/plain; charset=utf-8");
  }

  static void test_file_response_rejects_parent_traversal()
  {
    Response res;
    ResponseWrapper out{res};

    out.file("../secret.txt");

    assert(res.status() == vix::http::BAD_REQUEST);
    assert(res.body() == "Bad path");
    assert(res.header("Content-Type") == "text/plain; charset=utf-8");
  }

  static void test_file_response_directory_uses_index_html()
  {
    const auto dir = temp_test_dir() / "site";

    std::filesystem::create_directories(dir);
    write_file(dir / "index.html", "<!doctype html><html><body>home</body></html>");

    Response res;
    ResponseWrapper out{res};

    out.file(dir);

    assert(res.status() == vix::http::OK);
    assert(contains(res.body(), "home"));
    assert(res.header("Content-Type") == "text/html; charset=utf-8");
  }

  static void test_file_response_preserves_existing_cache_control()
  {
    const auto dir = temp_test_dir();
    const auto path = dir / "cache.txt";

    write_file(path, "cache");

    Response res;
    ResponseWrapper out{res};

    out.header("Cache-Control", "no-store")
        .file(path);

    assert(res.status() == vix::http::OK);
    assert(res.body() == "cache");
    assert(res.header("Cache-Control") == "no-store");
  }

  static void test_end_alias()
  {
    Response res;
    ResponseWrapper out{res};

    out.status(vix::http::NOT_FOUND).end();

    assert(res.status() == vix::http::NOT_FOUND);
    assert(res.body() == "404 Not Found");
  }

} // namespace

int main()
{
  test_wrapper_initializes_invalid_status_to_ok();
  test_status_and_status_alias();
  test_compile_time_status_helpers();
  test_header_set_append_and_type();
  test_text_response_sets_plain_text_defaults();
  test_text_response_preserves_existing_content_type();
  test_ui_html_response();
  test_ui_html_response_no_content_clears_body();
  test_ui_view_response();
  test_ui_view_preserves_existing_status();
  test_ui_view_requires_template_view();
  test_send_string_variants();
  test_send_without_payload_adds_default_status_body();
  test_send_without_payload_keeps_existing_body();
  test_send_no_content_clears_body();
  test_send_not_modified_clears_body();
  test_send_status();
  test_send_status_and_payload();
  test_send_compile_time_status_and_payload();
  test_json_response_with_vix_json();
  test_send_json_alias();
  test_json_response_preserves_existing_content_type();
  test_ordered_json_response_helper();
  test_json_ordered();
  test_token_to_json_primitives();
  test_array_to_json();
  test_kvs_to_json();
  test_kvs_to_json_ignores_non_string_keys();
  test_json_response_with_simple_kvs();
  test_json_response_with_simple_array();
  test_json_response_with_simple_token_object();
  test_send_simple_aliases();
  test_initializer_list_json();
  test_redirect_default_found();
  test_redirect_custom_status();
  test_location_header();
  test_default_status_message();
  test_mime_from_ext();
  test_read_file_binary();
  test_file_response_for_text_file();
  test_file_response_for_missing_file();
  test_file_response_rejects_parent_traversal();
  test_file_response_directory_uses_index_html();
  test_file_response_preserves_existing_cache_control();
  test_end_alias();

  return 0;
}
