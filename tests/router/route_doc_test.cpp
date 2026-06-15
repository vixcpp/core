/**
 *
 * @file route_doc_test.cpp
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
#include <string>
#include <vector>

#include <vix/router/RouteDoc.hpp>

namespace
{
  static void test_default_route_doc_is_empty()
  {
    vix::router::RouteDoc doc;

    assert(doc.summary.empty());
    assert(doc.description.empty());
    assert(doc.tags.empty());
    assert(doc.request_body.is_object());
    assert(doc.request_body.empty());
    assert(doc.responses.is_object());
    assert(doc.responses.empty());
    assert(doc.x.is_object());
    assert(doc.x.empty());

    assert(doc.empty());
  }

  static void test_summary_makes_doc_non_empty()
  {
    vix::router::RouteDoc doc;

    doc.summary = "List users";

    assert(!doc.empty());
    assert(doc.summary == "List users");
  }

  static void test_description_makes_doc_non_empty()
  {
    vix::router::RouteDoc doc;

    doc.description = "Return all registered users.";

    assert(!doc.empty());
    assert(doc.description == "Return all registered users.");
  }

  static void test_tags_make_doc_non_empty()
  {
    vix::router::RouteDoc doc;

    doc.tags.push_back("users");
    doc.tags.push_back("admin");

    assert(!doc.empty());
    assert(doc.tags.size() == 2);
    assert(doc.tags[0] == "users");
    assert(doc.tags[1] == "admin");
  }

  static void test_request_body_makes_doc_non_empty()
  {
    vix::router::RouteDoc doc;

    doc.request_body = {
        {"type", "object"},
        {"required", {"email", "password"}},
        {"properties",
         {
             {"email", {{"type", "string"}}},
             {"password", {{"type", "string"}}},
         }},
    };

    assert(!doc.empty());
    assert(doc.request_body["type"].get<std::string>() == "object");
    assert(doc.request_body["required"].is_array());
    assert(doc.request_body["required"][0].get<std::string>() == "email");
    assert(doc.request_body["required"][1].get<std::string>() == "password");
    assert(doc.request_body["properties"]["email"]["type"].get<std::string>() == "string");
    assert(doc.request_body["properties"]["password"]["type"].get<std::string>() == "string");
  }

  static void test_responses_make_doc_non_empty()
  {
    vix::router::RouteDoc doc;

    doc.responses = {
        {"200",
         {
             {"description", "User list returned"},
             {"content",
              {
                  {"application/json",
                   {
                       {"example",
                        {
                            {"ok", true},
                            {"count", 2},
                        }},
                   }},
              }},
         }},
        {"500",
         {
             {"description", "Internal Server Error"},
         }},
    };

    assert(!doc.empty());
    assert(doc.responses["200"]["description"].get<std::string>() == "User list returned");
    assert(doc.responses["200"]["content"]["application/json"]["example"]["ok"].get<bool>() == true);
    assert(doc.responses["200"]["content"]["application/json"]["example"]["count"].get<int>() == 2);
    assert(doc.responses["500"]["description"].get<std::string>() == "Internal Server Error");
  }

  static void test_vendor_extensions_make_doc_non_empty()
  {
    vix::router::RouteDoc doc;

    doc.x["x-vix-heavy"] = true;
    doc.x["x-vix-module"] = "core";

    assert(!doc.empty());
    assert(doc.x["x-vix-heavy"].get<bool>() == true);
    assert(doc.x["x-vix-module"].get<std::string>() == "core");
  }

  static void test_all_fields_can_be_used_together()
  {
    vix::router::RouteDoc doc;

    doc.summary = "Create user";
    doc.description = "Create a new user account.";
    doc.tags = {"users", "write"};

    doc.request_body = {
        {"type", "object"},
        {"required", {"email"}},
        {"properties",
         {
             {"email", {{"type", "string"}}},
         }},
    };

    doc.responses = {
        {"201",
         {
             {"description", "User created"},
         }},
    };

    doc.x = {
        {"x-vix-heavy", false},
        {"x-vix-runtime", "core"},
    };

    assert(!doc.empty());

    assert(doc.summary == "Create user");
    assert(doc.description == "Create a new user account.");

    assert(doc.tags.size() == 2);
    assert(doc.tags[0] == "users");
    assert(doc.tags[1] == "write");

    assert(doc.request_body["type"].get<std::string>() == "object");
    assert(doc.request_body["required"][0].get<std::string>() == "email");
    assert(doc.request_body["properties"]["email"]["type"].get<std::string>() == "string");

    assert(doc.responses["201"]["description"].get<std::string>() == "User created");

    assert(doc.x["x-vix-heavy"].get<bool>() == false);
    assert(doc.x["x-vix-runtime"].get<std::string>() == "core");
  }

  static void test_clearing_all_fields_makes_doc_empty_again()
  {
    vix::router::RouteDoc doc;

    doc.summary = "Temporary summary";
    doc.description = "Temporary description";
    doc.tags = {"temporary"};
    doc.request_body = {{"type", "object"}};
    doc.responses = {{"200", {{"description", "OK"}}}};
    doc.x = {{"x-vix-test", true}};

    assert(!doc.empty());

    doc.summary.clear();
    doc.description.clear();
    doc.tags.clear();
    doc.request_body = nlohmann::json::object();
    doc.responses = nlohmann::json::object();
    doc.x = nlohmann::json::object();

    assert(doc.empty());
  }

  static void test_empty_arrays_are_still_empty_for_json_fields()
  {
    vix::router::RouteDoc doc;

    doc.request_body = nlohmann::json::array();
    doc.responses = nlohmann::json::array();
    doc.x = nlohmann::json::array();

    assert(doc.request_body.is_array());
    assert(doc.responses.is_array());
    assert(doc.x.is_array());

    assert(doc.request_body.empty());
    assert(doc.responses.empty());
    assert(doc.x.empty());

    assert(doc.empty());
  }

  static void test_non_empty_arrays_make_doc_non_empty()
  {
    {
      vix::router::RouteDoc doc;

      doc.request_body = nlohmann::json::array({"email", "password"});

      assert(!doc.empty());
      assert(doc.request_body.is_array());
      assert(doc.request_body.size() == 2);
    }

    {
      vix::router::RouteDoc doc;

      doc.responses = nlohmann::json::array({"200", "404"});

      assert(!doc.empty());
      assert(doc.responses.is_array());
      assert(doc.responses.size() == 2);
    }

    {
      vix::router::RouteDoc doc;

      doc.x = nlohmann::json::array({"x-vix-test"});

      assert(!doc.empty());
      assert(doc.x.is_array());
      assert(doc.x.size() == 1);
    }
  }

} // namespace

int main()
{
  test_default_route_doc_is_empty();
  test_summary_makes_doc_non_empty();
  test_description_makes_doc_non_empty();
  test_tags_make_doc_non_empty();
  test_request_body_makes_doc_non_empty();
  test_responses_make_doc_non_empty();
  test_vendor_extensions_make_doc_non_empty();
  test_all_fields_can_be_used_together();
  test_clearing_all_fields_makes_doc_empty_again();
  test_empty_arrays_are_still_empty_for_json_fields();
  test_non_empty_arrays_make_doc_non_empty();

  return 0;
}
