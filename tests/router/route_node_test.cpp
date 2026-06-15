/**
 *
 * @file route_node_test.cpp
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
#include <type_traits>

#include <vix/http/IRequestHandler.hpp>
#include <vix/http/Request.hpp>
#include <vix/http/Response.hpp>
#include <vix/router/RouteNode.hpp>

namespace
{
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

  static std::shared_ptr<vix::http::IRequestHandler> make_handler()
  {
    return std::make_shared<DummyHandler>();
  }

  static void test_default_route_node_is_empty()
  {
    vix::router::RouteNode node;

    assert(node.children.empty());
    assert(!node.handler);
    assert(!node.has_handler());

    assert(node.isParam == false);
    assert(node.paramName.empty());
    assert(node.heavy == false);
  }

  static void test_has_child_returns_false_when_missing()
  {
    vix::router::RouteNode node;

    assert(!node.has_child("users"));
    assert(node.find_child("users") == nullptr);
  }

  static void test_get_or_create_child_creates_static_child()
  {
    vix::router::RouteNode node;

    vix::router::RouteNode &child = node.get_or_create_child("users");

    assert(node.has_child("users"));
    assert(node.children.size() == 1);
    assert(node.find_child("users") == &child);

    assert(!child.has_handler());
    assert(!child.isParam);
    assert(child.paramName.empty());
    assert(!child.heavy);
  }

  static void test_get_or_create_child_returns_existing_child()
  {
    vix::router::RouteNode node;

    vix::router::RouteNode &first = node.get_or_create_child("users");
    first.set_heavy(true);

    vix::router::RouteNode &second = node.get_or_create_child("users");

    assert(&first == &second);
    assert(node.children.size() == 1);
    assert(second.heavy == true);
  }

  static void test_multiple_static_children_are_independent()
  {
    vix::router::RouteNode node;

    auto &users = node.get_or_create_child("users");
    auto &posts = node.get_or_create_child("posts");
    auto &health = node.get_or_create_child("health");

    users.set_heavy(true);
    posts.mark_as_param("post_id");

    assert(node.children.size() == 3);

    assert(node.find_child("users") == &users);
    assert(node.find_child("posts") == &posts);
    assert(node.find_child("health") == &health);

    assert(users.heavy == true);
    assert(posts.isParam == true);
    assert(posts.paramName == "post_id");
    assert(health.heavy == false);
  }

  static void test_const_find_child()
  {
    vix::router::RouteNode node;

    vix::router::RouteNode &child = node.get_or_create_child("users");
    child.set_heavy(true);

    const vix::router::RouteNode &const_node = node;

    const vix::router::RouteNode *found = const_node.find_child("users");

    assert(found != nullptr);
    assert(found->heavy == true);
    assert(const_node.find_child("missing") == nullptr);
  }

  static void test_nested_children()
  {
    vix::router::RouteNode root;

    auto &api = root.get_or_create_child("api");
    auto &users = api.get_or_create_child("users");
    auto &id = users.get_or_create_child("*");

    id.mark_as_param("id");

    assert(root.has_child("api"));
    assert(api.has_child("users"));
    assert(users.has_child("*"));

    assert(root.find_child("api") == &api);
    assert(root.find_child("api")->find_child("users") == &users);
    assert(root.find_child("api")->find_child("users")->find_child("*") == &id);

    assert(id.isParam == true);
    assert(id.paramName == "id");
  }

  static void test_mark_as_param()
  {
    vix::router::RouteNode node;

    assert(!node.isParam);
    assert(node.paramName.empty());

    node.mark_as_param("id");

    assert(node.isParam);
    assert(node.paramName == "id");
  }

  static void test_mark_as_param_can_replace_param_name()
  {
    vix::router::RouteNode node;

    node.mark_as_param("id");

    assert(node.isParam);
    assert(node.paramName == "id");

    node.mark_as_param("slug");

    assert(node.isParam);
    assert(node.paramName == "slug");
  }

  static void test_set_handler()
  {
    vix::router::RouteNode node;

    assert(!node.has_handler());

    auto handler = make_handler();

    node.set_handler(handler);

    assert(node.has_handler());
    assert(node.handler == handler);
  }

  static void test_set_handler_accepts_nullptr()
  {
    vix::router::RouteNode node;

    node.set_handler(make_handler());

    assert(node.has_handler());

    node.set_handler(nullptr);

    assert(!node.has_handler());
    assert(!node.handler);
  }

  static void test_set_heavy()
  {
    vix::router::RouteNode node;

    assert(node.heavy == false);

    node.set_heavy(true);

    assert(node.heavy == true);

    node.set_heavy(false);

    assert(node.heavy == false);
  }

  static void test_param_child_with_handler_and_heavy_flag()
  {
    vix::router::RouteNode root;

    auto &users = root.get_or_create_child("users");
    auto &id = users.get_or_create_child("*");

    id.mark_as_param("id");
    id.set_heavy(true);
    id.set_handler(make_handler());

    assert(root.has_child("users"));
    assert(users.has_child("*"));

    assert(id.isParam);
    assert(id.paramName == "id");
    assert(id.heavy);
    assert(id.has_handler());
  }

  static void test_move_constructor_preserves_node_data()
  {
    vix::router::RouteNode source;

    source.get_or_create_child("users");
    source.mark_as_param("id");
    source.set_heavy(true);

    auto handler = make_handler();
    source.set_handler(handler);

    vix::router::RouteNode moved = std::move(source);

    assert(moved.has_child("users"));
    assert(moved.isParam);
    assert(moved.paramName == "id");
    assert(moved.heavy);
    assert(moved.has_handler());
    assert(moved.handler == handler);
  }

  static void test_move_assignment_preserves_node_data()
  {
    vix::router::RouteNode source;

    source.get_or_create_child("projects");
    source.mark_as_param("slug");
    source.set_heavy(true);

    auto handler = make_handler();
    source.set_handler(handler);

    vix::router::RouteNode target;
    target.get_or_create_child("old");
    target.set_heavy(false);

    target = std::move(source);

    assert(target.has_child("projects"));
    assert(!target.has_child("old"));

    assert(target.isParam);
    assert(target.paramName == "slug");
    assert(target.heavy);
    assert(target.has_handler());
    assert(target.handler == handler);
  }

  static void test_type_traits()
  {
    static_assert(std::is_default_constructible_v<vix::router::RouteNode>);
    static_assert(!std::is_copy_constructible_v<vix::router::RouteNode>);
    static_assert(!std::is_copy_assignable_v<vix::router::RouteNode>);
    static_assert(std::is_move_constructible_v<vix::router::RouteNode>);
    static_assert(std::is_move_assignable_v<vix::router::RouteNode>);
  }

} // namespace

int main()
{
  test_default_route_node_is_empty();
  test_has_child_returns_false_when_missing();
  test_get_or_create_child_creates_static_child();
  test_get_or_create_child_returns_existing_child();
  test_multiple_static_children_are_independent();
  test_const_find_child();
  test_nested_children();
  test_mark_as_param();
  test_mark_as_param_can_replace_param_name();
  test_set_handler();
  test_set_handler_accepts_nullptr();
  test_set_heavy();
  test_param_child_with_handler_and_heavy_flag();
  test_move_constructor_preserves_node_data();
  test_move_assignment_preserves_node_data();
  test_type_traits();

  return 0;
}
