#include <vix/utils/NetworkError.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <system_error>

namespace
{
  int failures = 0;

  void expect_true(bool value, const std::string &name)
  {
    if (!value)
    {
      std::cerr << "FAILED: expected true: " << name << "\n";
      ++failures;
    }
  }

  void expect_false(bool value, const std::string &name)
  {
    if (value)
    {
      std::cerr << "FAILED: expected false: " << name << "\n";
      ++failures;
    }
  }

  void test_response_write_disconnect_messages()
  {
    expect_true(
        vix::utils::is_normal_network_disconnect_message("Broken pipe"),
        "Broken pipe");

    expect_true(
        vix::utils::is_normal_network_disconnect_message("Connection reset by peer"),
        "Connection reset by peer");

    expect_true(
        vix::utils::is_normal_network_disconnect_message("Operation canceled"),
        "Operation canceled");

    expect_true(
        vix::utils::is_normal_network_disconnect_message("Operation cancelled"),
        "Operation cancelled");

    expect_true(
        vix::utils::is_normal_network_disconnect_message("End of file"),
        "End of file");

    expect_true(
        vix::utils::is_normal_network_disconnect_message("EOF"),
        "EOF");
  }

  void test_unexpected_write_errors_stay_visible()
  {
    expect_false(
        vix::utils::is_normal_network_disconnect_message("permission denied"),
        "permission denied");

    expect_false(
        vix::utils::is_normal_network_disconnect_message("invalid response state"),
        "invalid response state");

    expect_false(
        vix::utils::is_normal_network_disconnect_message("serialization failed"),
        "serialization failed");

    expect_false(
        vix::utils::is_normal_network_disconnect_message("HTTP header too large"),
        "HTTP header too large");
  }

  void test_system_error_disconnects()
  {
    expect_true(
        vix::utils::is_normal_network_disconnect(
            std::system_error(std::make_error_code(std::errc::broken_pipe))),
        "std::errc::broken_pipe");

    expect_true(
        vix::utils::is_normal_network_disconnect(
            std::system_error(std::make_error_code(std::errc::connection_reset))),
        "std::errc::connection_reset");

    expect_true(
        vix::utils::is_normal_network_disconnect(
            std::system_error(std::make_error_code(std::errc::operation_canceled))),
        "std::errc::operation_canceled");

    expect_true(
        vix::utils::is_normal_network_disconnect(
            std::system_error(std::make_error_code(std::errc::timed_out))),
        "std::errc::timed_out");
  }

  void test_unexpected_system_errors_stay_visible()
  {
    expect_false(
        vix::utils::is_normal_network_disconnect(
            std::system_error(std::make_error_code(std::errc::permission_denied))),
        "std::errc::permission_denied");

    expect_false(
        vix::utils::is_normal_network_disconnect(
            std::system_error(std::make_error_code(std::errc::invalid_argument))),
        "std::errc::invalid_argument");
  }
}

int main()
{
  test_response_write_disconnect_messages();
  test_unexpected_write_errors_stay_visible();
  test_system_error_disconnects();
  test_unexpected_system_errors_stay_visible();

  if (failures != 0)
  {
    std::cerr << "session_disconnect_tests failed with "
              << failures
              << " failure(s)\n";

    return EXIT_FAILURE;
  }

  std::cout << "session_disconnect_tests passed\n";
  return EXIT_SUCCESS;
}
