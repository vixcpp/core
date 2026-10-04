/**
 * @file server_ready_presentation_test.cpp
 * @brief Regression coverage for HTTP/server readiness presentation.
 */

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

#include <vix/log/ConsoleSync.hpp>
#include <vix/server/ServerReadyPresentation.hpp>

namespace
{
  class ScopedEnvironment final
  {
  public:
    ScopedEnvironment(const char *name, std::optional<std::string> value)
        : name_(name), original_(read(name))
    {
      set(std::move(value));
    }

    ScopedEnvironment(const ScopedEnvironment &) = delete;
    ScopedEnvironment &operator=(const ScopedEnvironment &) = delete;

    ~ScopedEnvironment()
    {
      set(original_);
    }

  private:
    static std::optional<std::string> read(const char *name)
    {
      if (const char *value = std::getenv(name); value != nullptr)
        return std::string{value};
      return std::nullopt;
    }

    void set(const std::optional<std::string> &value) const
    {
#if defined(_WIN32)
      const int result = _putenv_s(name_, value ? value->c_str() : "");
#else
      const int result = value ? setenv(name_, value->c_str(), 1) : unsetenv(name_);
#endif
      assert(result == 0);
    }

    const char *name_;
    std::optional<std::string> original_;
  };

  class CerrCapture final
  {
  public:
    CerrCapture()
        : previous_(std::cerr.rdbuf(stream_.rdbuf()))
    {
    }

    CerrCapture(const CerrCapture &) = delete;
    CerrCapture &operator=(const CerrCapture &) = delete;

    ~CerrCapture()
    {
      std::cerr.rdbuf(previous_);
    }

    [[nodiscard]] std::string str() const
    {
      return stream_.str();
    }

  private:
    std::ostringstream stream_;
    std::streambuf *previous_;
  };

  void test_ready_info_defaults()
  {
    const vix::server::ServerReadyInfo info{};

    assert(info.app == "vix.cpp");
    assert(info.ready_ms == -1);
    assert(info.status == "ready");
    assert(info.host == "localhost");
    assert(info.port == 8080);
    assert(info.scheme == "http");
    assert(info.base_path == "/");
    assert(info.show_ws);
    assert(info.ws_host == "localhost");
    assert(info.ws_port == 9090);
    assert(info.ws_scheme == "ws");
    assert(info.ws_path == "/");
    assert(info.show_hints);
  }

  void test_mode_policy()
  {
    ScopedEnvironment mode{"VIX_MODE", std::nullopt};
    assert(vix::server::StartupPresentation::mode_from_env() == "run");

    {
      ScopedEnvironment value{"VIX_MODE", ""};
      assert(vix::server::StartupPresentation::mode_from_env() == "run");
    }
    {
      ScopedEnvironment value{"VIX_MODE", "DEV"};
      assert(vix::server::StartupPresentation::mode_from_env() == "dev");
    }
    {
      ScopedEnvironment value{"VIX_MODE", "watch"};
      assert(vix::server::StartupPresentation::mode_from_env() == "dev");
    }
    {
      ScopedEnvironment value{"VIX_MODE", "reload"};
      assert(vix::server::StartupPresentation::mode_from_env() == "dev");
    }
    {
      ScopedEnvironment value{"VIX_MODE", " dev "};
      assert(vix::server::StartupPresentation::mode_from_env() == "run");
    }
  }

  void test_color_and_hyperlink_policy()
  {
    ScopedEnvironment no_color{"NO_COLOR", std::nullopt};
    ScopedEnvironment color{"VIX_COLOR", "never"};
    assert(!vix::server::StartupPresentation::colors_enabled());

    {
      ScopedEnvironment value{"VIX_COLOR", "ALWAYS"};
      assert(vix::server::StartupPresentation::colors_enabled());
    }
    {
      ScopedEnvironment value{"NO_COLOR", "1"};
      assert(!vix::server::StartupPresentation::colors_enabled());
    }
    {
      ScopedEnvironment disable{"VIX_NO_HYPERLINK", "1"};
      assert(!vix::server::StartupPresentation::hyperlinks_enabled());
    }

    const std::string url{"https://example.test/docs"};
    assert(vix::server::StartupPresentation::osc8_link(url, "docs", false) == "docs");
    assert(vix::server::StartupPresentation::osc8_link(url, "docs", true) ==
           "\033]8;;https://example.test/docs\033\\docs\033]8;;\033\\");
  }

  void test_stderr_rendering_and_endpoint_choices()
  {
    ScopedEnvironment no_color{"NO_COLOR", "1"};
    ScopedEnvironment color{"VIX_COLOR", std::nullopt};
    ScopedEnvironment hyperlinks{"VIX_NO_HYPERLINK", "1"};
    ScopedEnvironment animation{"VIX_NO_ANIM", "1"};

    vix::server::ServerReadyInfo info{};
    info.app = "Audit Server";
    info.version = "v3";
    info.ready_ms = 12;
    info.mode = "dev";
    info.status = "listening";
    info.config_path = "/tmp/audit.json";
    info.scheme = "https";
    info.host = "example.test";
    info.port = 8443;
    info.base_path = "api";
    info.show_ws = true;
    info.ws_scheme = "wss";
    info.ws_host = "ws.example.test";
    info.ws_port = 9443;
    info.ws_path = "socket";
    info.threads = 2;
    info.max_threads = 4;
    info.show_hints = true;

    std::string output;
    {
      CerrCapture capture;
      vix::server::StartupPresentation::emit_server_ready(info);
      output = capture.str();
    }

    assert(output.find("[Audit Server]") != std::string::npos);
    assert(output.find("LISTENING") != std::string::npos);
    assert(output.find("v3") != std::string::npos);
    assert(output.find("(12 ms)") != std::string::npos);
    assert(output.find("[dev]") != std::string::npos);
    assert(output.find("https://example.test:8443/api") != std::string::npos);
    assert(output.find("wss://ws.example.test:9443/socket") != std::string::npos);
    assert(output.find("Config:") != std::string::npos);
    assert(output.find("/tmp/audit.json") != std::string::npos);
    assert(output.find("Threads:") != std::string::npos);
    assert(output.find("2/4") != std::string::npos);
    assert(output.find("Mode:") != std::string::npos);
    assert(output.find("dev (watch/reload)") != std::string::npos);
    assert(output.find("Status:") != std::string::npos);
    assert(output.find("listening") != std::string::npos);
    assert(output.find("Ctrl+C to stop the server") != std::string::npos);
    assert(vix::log::console_banner_done());
  }

  void test_animation_disable_policy()
  {
    ScopedEnvironment no_color{"NO_COLOR", std::nullopt};
    ScopedEnvironment color{"VIX_COLOR", "always"};
    ScopedEnvironment animation{"VIX_NO_ANIM", "1"};
    ScopedEnvironment hyperlinks{"VIX_NO_HYPERLINK", "1"};

    vix::server::ServerReadyInfo info{};
    info.app = "Animation Policy";
    info.mode = "dev";
    info.show_ws = false;
    info.show_hints = false;

    std::string output;
    {
      CerrCapture capture;
      vix::server::StartupPresentation::emit_server_ready(info);
      output = capture.str();
    }

    // With color enabled, VIX_NO_ANIM selects the stable development tag
    // instead of the wall-clock-driven animated variant.
    assert(output.find("[dev]") != std::string::npos);
  }

  void test_optional_rows_are_omitted()
  {
    ScopedEnvironment no_color{"NO_COLOR", "1"};
    ScopedEnvironment color{"VIX_COLOR", std::nullopt};
    ScopedEnvironment hyperlinks{"VIX_NO_HYPERLINK", "1"};

    vix::server::ServerReadyInfo info{};
    info.app = "Minimal";
    info.scheme = "http";
    info.host = "127.0.0.1";
    info.port = 8081;
    info.base_path.clear();
    info.show_ws = false;
    info.config_path.clear();
    info.threads = 0;
    info.show_hints = false;

    std::string output;
    {
      CerrCapture capture;
      vix::server::StartupPresentation::emit_server_ready(info);
      output = capture.str();
    }

    assert(output.find("http://127.0.0.1:8081/") != std::string::npos);
    assert(output.find("WS:") == std::string::npos);
    assert(output.find("Config:") == std::string::npos);
    assert(output.find("Threads:") == std::string::npos);
    assert(output.find("Ctrl+C to stop the server") == std::string::npos);
  }
} // namespace

int main()
{
  test_ready_info_defaults();
  test_mode_policy();
  test_color_and_hyperlink_policy();
  test_stderr_rendering_and_endpoint_choices();
  test_animation_disable_policy();
  test_optional_rows_are_omitted();
  return 0;
}
