/**
 * @file app_env_test.cpp
 * @brief Regression coverage for App's canonical vix::env consumption.
 */
#include <cassert>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

#include <vix/app/App.hpp>
#include <vix/router/Router.hpp>

namespace
{
  class ScopedEnvironment
  {
  public:
    explicit ScopedEnvironment(std::string name)
        : name_(std::move(name))
    {
      if (const char *value = std::getenv(name_.c_str()))
      {
        was_set_ = true;
        previous_ = value;
      }
    }

    ~ScopedEnvironment()
    {
      if (was_set_)
      {
        set(previous_);
      }
      else
      {
        unset();
      }
    }

    ScopedEnvironment(const ScopedEnvironment &) = delete;
    ScopedEnvironment &operator=(const ScopedEnvironment &) = delete;

    void set(std::string_view value)
    {
      const std::string stable_value(value);
#if defined(_WIN32)
      assert(_putenv_s(name_.c_str(), stable_value.c_str()) == 0);
#else
      assert(setenv(name_.c_str(), stable_value.c_str(), 1) == 0);
#endif
    }

    void unset()
    {
#if defined(_WIN32)
      assert(_putenv_s(name_.c_str(), "") == 0);
#else
      assert(unsetenv(name_.c_str()) == 0);
#endif
    }

  private:
    std::string name_;
    bool was_set_{false};
    std::string previous_;
  };

  void assert_docs_enabled(bool expected)
  {
    vix::App app;
    assert(app.router() != nullptr);
    assert(app.router()->has_route("GET", "/openapi.json") == expected);
    app.close();
  }

  void test_app_environment_contract()
  {
    ScopedEnvironment docs("VIX_DOCS");
    ScopedEnvironment access_logs("VIX_ACCESS_LOGS");
    ScopedEnvironment internal_logs("VIX_INTERNAL_LOGS");
    ScopedEnvironment log_async("VIX_LOG_ASYNC");
    ScopedEnvironment log_level("VIX_LOG_LEVEL");
    ScopedEnvironment mode("VIX_MODE");
    ScopedEnvironment silent("VIX_ENV_SILENT");

    // Keep constructor output and the one-time access-log middleware quiet.
    access_logs.set("false");
    internal_logs.set("false");
    log_async.set("false");
    log_level.set("critical");
    mode.unset();
    silent.set("true");

    docs.unset();
    assert_docs_enabled(true);

    docs.set("false");
    assert_docs_enabled(false);

    docs.set("true");
    assert_docs_enabled(true);

    docs.set("not-a-bool");
    assert_docs_enabled(false);

    docs.set("");
#if defined(_WIN32)
    // Vix2 treated an empty Windows variable as absent, retaining the default.
    assert_docs_enabled(true);
#else
    // On POSIX it is present but invalid, so the historical parser returned false.
    assert_docs_enabled(false);
#endif
  }
} // namespace

int main()
{
  test_app_environment_contract();
  return 0;
}
