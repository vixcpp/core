/**
 *
 * @file Config.hpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */
#ifndef VIX_CONFIG_HPP
#define VIX_CONFIG_HPP

#include <memory>
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace vix::config
{

  /**
   * @brief Global configuration loader and accessor.
   *
   * Loads configuration from a JSON file and exposes typed accessors.
   * Implemented as a singleton.
   */
  class Config
  {
  public:
    /** @brief Construct a configuration with an optional config file path. */
    explicit Config(const std::filesystem::path &configPath = "");

    Config(const Config &) = delete;
    Config &operator=(const Config &) = delete;

    /** @brief Get the singleton instance (lazy-initialized). */
    static Config &getInstance(const std::filesystem::path &configPath = "");

    /** @brief Load or reload the configuration file. */
    void loadConfig();

#if VIX_CORE_WITH_MYSQL
    namespace sql
    {
      class Connection;
    }
    /** @brief Get a database connection if MySQL support is enabled. */
    std::shared_ptr<sql::Connection> getDbConnection();
#endif

    /** @brief Read database password from environment variables. */
    std::string getDbPasswordFromEnv();

    /** @brief Database host. */
    const std::string &getDbHost() const noexcept;

    /** @brief Database user. */
    const std::string &getDbUser() const noexcept;

    /** @brief Database name. */
    const std::string &getDbName() const noexcept;

    /** @brief Database port. */
    int getDbPort() const noexcept;

    /** @brief HTTP server port. */
    int getServerPort() const noexcept;

    /** @brief Request timeout in milliseconds. */
    int getRequestTimeout() const noexcept;

    /** @brief Set the HTTP server port. */
    void setServerPort(int port);

    /** @brief Check whether a dotted configuration key exists. */
    bool has(const std::string &dottedKey) const noexcept;

    /** @brief Get an integer value with a default fallback. */
    int getInt(const std::string &dottedKey, int defaultValue) const noexcept;

    /** @brief Get a boolean value with a default fallback. */
    bool getBool(const std::string &dottedKey, bool defaultValue) const noexcept;

    /** @brief Get a string value with a default fallback. */
    std::string getString(
        const std::string &dottedKey,
        const std::string &defaultValue) const noexcept;

    /** @brief Number of IO threads (0 means auto). */
    int getIOThreads() const noexcept;

    /** @brief Whether benchmark mode is enabled. */
    bool isBenchMode() const noexcept;

    /** @brief Whether async logging is enabled. */
    bool getLogAsync() const noexcept;

    /** @brief Maximum async log queue size. */
    int getLogQueueMax() const noexcept;

    /** @brief Whether to drop logs on overflow. */
    bool getLogDropOnOverflow() const noexcept;

    /** @brief WAF mode ("off", "basic", or "strict"). */
    const std::string &getWafMode() const noexcept;

    /** @brief Maximum WAF target length. */
    int getWafMaxTargetLen() const noexcept;

    /** @brief Maximum WAF body size in bytes. */
    int getWafMaxBodyBytes() const noexcept;

    /** @brief Session timeout in seconds. */
    int getSessionTimeoutSec() const noexcept;

  private:
    static constexpr const char *DEFAULT_DB_HOST = "localhost";
    static constexpr const char *DEFAULT_DB_USER = "root";
    static constexpr const char *DEFAULT_DB_PASS = "";
    static constexpr const char *DEFAULT_DB_NAME = "";
    static constexpr int DEFAULT_DB_PORT = 3306;
    static constexpr int DEFAULT_SERVER_PORT = 8080;
    static constexpr int DEFAULT_REQUEST_TIMEOUT = 2000; // ms
    std::filesystem::path configPath_;

    std::string db_host;
    std::string db_user;
    std::string db_pass;
    std::string db_name;

    int db_port;
    int server_port;
    int request_timeout;
    nlohmann::json rawConfig_;
    const nlohmann::json *findNode(const std::string &dottedKey) const noexcept;
    static constexpr int DEFAULT_IO_THREADS = 0; // 0 => auto
    static constexpr bool DEFAULT_LOG_ASYNC = true;
    static constexpr int DEFAULT_LOG_QUEUE_MAX = 20000;
    static constexpr bool DEFAULT_LOG_DROP_ON_OVERFLOW = true;
    static constexpr const char *DEFAULT_WAF_MODE = "basic"; // off|basic|strict
    static constexpr int DEFAULT_WAF_MAX_TARGET_LEN = 4096;
    static constexpr int DEFAULT_WAF_MAX_BODY_BYTES = 1024 * 1024;
    int io_threads_;
    bool log_async_;
    int log_queue_max_;
    bool log_drop_on_overflow_;

    std::string waf_mode_;
    int waf_max_target_len_;
    int waf_max_body_bytes_;

    static constexpr int DEFAULT_SESSION_TIMEOUT_SEC = 20;
    int session_timeout_sec_;
  };

} // namespace vix::config

#endif // VIX_CONFIG_HPP
