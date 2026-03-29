/**
 *
 *  @file format_test.cpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira. All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
 *
 */

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <vix/format.hpp>

namespace
{
  struct Point
  {
    int x = 0;
    int y = 0;
  };

  inline void vix_format(std::ostream &os, const Point &p)
  {
    os << "Point{x=" << p.x << ", y=" << p.y << "}";
  }

  void assert_equal(const std::string &actual,
                    const std::string &expected,
                    const char *test_name)
  {
    if (actual != expected)
    {
      std::cerr << "[FAIL] " << test_name << '\n';
      std::cerr << "  expected: " << expected << '\n';
      std::cerr << "  actual  : " << actual << '\n';
      std::abort();
    }
  }

  void assert_true(bool condition, const char *test_name)
  {
    if (!condition)
    {
      std::cerr << "[FAIL] " << test_name << '\n';
      std::abort();
    }
  }

  template <typename Fn>
  void assert_throws(Fn &&fn, const char *test_name)
  {
    bool thrown = false;

    try
    {
      fn();
    }
    catch (const vix::format_error &)
    {
      thrown = true;
    }
    catch (...)
    {
      std::cerr << "[FAIL] " << test_name << '\n';
      std::cerr << "  unexpected exception type thrown\n";
      std::abort();
    }

    if (!thrown)
    {
      std::cerr << "[FAIL] " << test_name << '\n';
      std::cerr << "  expected vix::format_error but no exception was thrown\n";
      std::abort();
    }
  }

  void test_returns_literal_when_no_placeholders_exist()
  {
    const std::string result = vix::format("Hello world");
    assert_equal(result, "Hello world",
                 "test_returns_literal_when_no_placeholders_exist");
  }

  void test_formats_single_automatic_placeholder()
  {
    const std::string result = vix::format("Hello, {}", "world");
    assert_equal(result, "Hello, \"world\"",
                 "test_formats_single_automatic_placeholder");
  }

  void test_formats_multiple_automatic_placeholders()
  {
    const std::string result = vix::format("{} + {} = {}", 2, 2, 4);
    assert_equal(result, "2 + 2 = 4",
                 "test_formats_multiple_automatic_placeholders");
  }

  void test_formats_explicit_indexes()
  {
    const std::string result = vix::format("{0} + {0} = {1}", 2, 4);
    assert_equal(result, "2 + 2 = 4",
                 "test_formats_explicit_indexes");
  }

  void test_formats_explicit_indexes_out_of_order()
  {
    const std::string result = vix::format("{2} {1} {0}", "A", "B", "C");
    assert_equal(result, "\"C\" \"B\" \"A\"",
                 "test_formats_explicit_indexes_out_of_order");
  }

  void test_reuses_explicit_argument_multiple_times()
  {
    const std::string result = vix::format("{0}-{0}-{0}", "x");
    assert_equal(result, "\"x\"-\"x\"-\"x\"",
                 "test_reuses_explicit_argument_multiple_times");
  }

  void test_supports_escaped_opening_brace()
  {
    const std::string result = vix::format("{{ value");
    assert_equal(result, "{ value",
                 "test_supports_escaped_opening_brace");
  }

  void test_supports_escaped_closing_brace()
  {
    const std::string result = vix::format("value }}");
    assert_equal(result, "value }",
                 "test_supports_escaped_closing_brace");
  }

  void test_supports_escaped_braces_around_placeholder()
  {
    const std::string result = vix::format("{{ status = {} }}", "ok");
    assert_equal(result, "{ status = \"ok\" }",
                 "test_supports_escaped_braces_around_placeholder");
  }

  void test_supports_empty_format_string()
  {
    const std::string result = vix::format("");
    assert_true(result.empty(),
                "test_supports_empty_format_string");
  }

  void test_supports_empty_string_argument()
  {
    const std::string result = vix::format("value=[{}]", "");
    assert_equal(result, "value=[\"\"]",
                 "test_supports_empty_string_argument");
  }

  void test_supports_boolean_values()
  {
    const std::string result = vix::format("{} {}", true, false);
    assert_equal(result, "true false",
                 "test_supports_boolean_values");
  }

  void test_supports_character_values()
  {
    const std::string result = vix::format("{} {} {}", 'A', 'B', 'C');
    assert_equal(result, "'A' 'B' 'C'",
                 "test_supports_character_values");
  }

  void test_supports_integer_values()
  {
    const std::string result = vix::format("{}", 42);
    assert_equal(result, "42",
                 "test_supports_integer_values");
  }

  void test_supports_floating_point_values()
  {
    const std::string result = vix::format("{} {}", 3.5f, 2.25);
    assert_equal(result, "3.5 2.25",
                 "test_supports_floating_point_values");
  }

  void test_supports_string_values()
  {
    const std::string result = vix::format("{} {}", std::string("Ada"), "Lovelace");
    assert_equal(result, "\"Ada\" \"Lovelace\"",
                 "test_supports_string_values");
  }

  void test_supports_vector_values_using_vix_rendering()
  {
    const std::string result = vix::format("{}", std::vector<int>{1, 2, 3});
    assert_equal(result, "[1, 2, 3]",
                 "test_supports_vector_values_using_vix_rendering");
  }

  void test_supports_map_values_using_vix_rendering()
  {
    const std::string result =
        vix::format("{}", std::map<std::string, int>{{"a", 1}, {"b", 2}});

    assert_true(!result.empty(),
                "test_supports_map_values_using_vix_rendering_not_empty");
    assert_true(result.find("\"a\" => 1") != std::string::npos,
                "test_supports_map_values_using_vix_rendering_contains_a");
    assert_true(result.find("\"b\" => 2") != std::string::npos,
                "test_supports_map_values_using_vix_rendering_contains_b");
    assert_true(result.front() == '{',
                "test_supports_map_values_using_vix_rendering_open_brace");
    assert_true(result.back() == '}',
                "test_supports_map_values_using_vix_rendering_close_brace");
  }

  void test_supports_user_defined_types_through_adl_vix_format()
  {
    const std::string result = vix::format("{}", Point{3, 7});
    assert_equal(result, "Point{x=3, y=7}",
                 "test_supports_user_defined_types_through_adl_vix_format");
  }

  void test_supports_mixed_types()
  {
    const std::string result = vix::format("name={} age={} active={}", "Ada", 28, true);
    assert_equal(result, "name=\"Ada\" age=28 active=true",
                 "test_supports_mixed_types");
  }

  void test_throws_on_unmatched_opening_brace()
  {
    assert_throws(
        []()
        {
          (void)vix::format("Hello {", "world");
        },
        "test_throws_on_unmatched_opening_brace");
  }

  void test_throws_on_single_closing_brace()
  {
    assert_throws(
        []()
        {
          (void)vix::format("Hello }", "world");
        },
        "test_throws_on_single_closing_brace");
  }

  void test_throws_on_invalid_explicit_index()
  {
    assert_throws(
        []()
        {
          (void)vix::format("{abc}", 42);
        },
        "test_throws_on_invalid_explicit_index");
  }

  void test_throws_on_empty_explicit_index_token_with_spaces()
  {
    assert_throws(
        []()
        {
          (void)vix::format("{ }", 42);
        },
        "test_throws_on_empty_explicit_index_token_with_spaces");
  }

  void test_throws_when_explicit_index_is_out_of_range()
  {
    assert_throws(
        []()
        {
          (void)vix::format("{1}", 42);
        },
        "test_throws_when_explicit_index_is_out_of_range");
  }

  void test_throws_when_automatic_index_exceeds_argument_count()
  {
    assert_throws(
        []()
        {
          (void)vix::format("{} {}", 42);
        },
        "test_throws_when_automatic_index_exceeds_argument_count");
  }

  void test_throws_when_mixing_automatic_and_explicit_indexing_auto_then_explicit()
  {
    assert_throws(
        []()
        {
          (void)vix::format("{} {0}", 42);
        },
        "test_throws_when_mixing_automatic_and_explicit_indexing_auto_then_explicit");
  }

  void test_throws_when_mixing_automatic_and_explicit_indexing_explicit_then_auto()
  {
    assert_throws(
        []()
        {
          (void)vix::format("{0} {}", 42);
        },
        "test_throws_when_mixing_automatic_and_explicit_indexing_explicit_then_auto");
  }

  void test_throws_when_specifier_syntax_is_used()
  {
    assert_throws(
        []()
        {
          (void)vix::format("{:>10}", 42);
        },
        "test_throws_when_specifier_syntax_is_used");
  }

  void test_format_append_appends_to_existing_string()
  {
    std::string out = "prefix:";
    vix::format_append(out, " {} {}", 10, 20);
    assert_equal(out, "prefix: 10 20",
                 "test_format_append_appends_to_existing_string");
  }

  void test_format_append_supports_escaped_braces()
  {
    std::string out;
    vix::format_append(out, "{{{}}}", 42);
    assert_equal(out, "{42}",
                 "test_format_append_supports_escaped_braces");
  }

  void test_format_to_replaces_previous_content()
  {
    std::string out = "old value";
    vix::format_to(out, "new {}", 123);
    assert_equal(out, "new 123",
                 "test_format_to_replaces_previous_content");
  }

  void test_format_to_supports_empty_result()
  {
    std::string out = "old";
    vix::format_to(out, "");
    assert_true(out.empty(),
                "test_format_to_supports_empty_result");
  }

  void test_format_append_can_be_called_repeatedly()
  {
    std::string out;
    vix::format_append(out, "A");
    vix::format_append(out, " {}", 1);
    vix::format_append(out, " {}", 2);
    vix::format_append(out, " {}", 3);

    assert_equal(out, "A 1 2 3",
                 "test_format_append_can_be_called_repeatedly");
  }

  void test_works_with_zero_arguments_and_escaped_braces()
  {
    const std::string result = vix::format("{{}}");
    assert_equal(result, "{}",
                 "test_works_with_zero_arguments_and_escaped_braces");
  }

  void test_works_with_large_explicit_index_values_that_are_valid()
  {
    const std::string result = vix::format("{3}-{2}-{1}-{0}", "a", "b", "c", "d");
    assert_equal(result, "\"d\"-\"c\"-\"b\"-\"a\"",
                 "test_works_with_large_explicit_index_values_that_are_valid");
  }

  void run_all_tests()
  {
    test_returns_literal_when_no_placeholders_exist();
    test_formats_single_automatic_placeholder();
    test_formats_multiple_automatic_placeholders();
    test_formats_explicit_indexes();
    test_formats_explicit_indexes_out_of_order();
    test_reuses_explicit_argument_multiple_times();
    test_supports_escaped_opening_brace();
    test_supports_escaped_closing_brace();
    test_supports_escaped_braces_around_placeholder();
    test_supports_empty_format_string();
    test_supports_empty_string_argument();
    test_supports_boolean_values();
    test_supports_character_values();
    test_supports_integer_values();
    test_supports_floating_point_values();
    test_supports_string_values();
    test_supports_vector_values_using_vix_rendering();
    test_supports_map_values_using_vix_rendering();
    test_supports_user_defined_types_through_adl_vix_format();
    test_supports_mixed_types();
    test_throws_on_unmatched_opening_brace();
    test_throws_on_single_closing_brace();
    test_throws_on_invalid_explicit_index();
    test_throws_on_empty_explicit_index_token_with_spaces();
    test_throws_when_explicit_index_is_out_of_range();
    test_throws_when_automatic_index_exceeds_argument_count();
    test_throws_when_mixing_automatic_and_explicit_indexing_auto_then_explicit();
    test_throws_when_mixing_automatic_and_explicit_indexing_explicit_then_auto();
    test_throws_when_specifier_syntax_is_used();
    test_format_append_appends_to_existing_string();
    test_format_append_supports_escaped_braces();
    test_format_to_replaces_previous_content();
    test_format_to_supports_empty_result();
    test_format_append_can_be_called_repeatedly();
    test_works_with_zero_arguments_and_escaped_braces();
    test_works_with_large_explicit_index_values_that_are_valid();
  }

} // namespace

int main()
{
  run_all_tests();
  std::cout << "[OK] All format tests passed.\n";
  return 0;
}
