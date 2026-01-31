/**
 *
 *  @file Config.hpp
 *  @author Gaspard Kirira
 *
 *  Copyright 2025, Gaspard Kirira.  All rights reserved.
 *  https://github.com/vixcpp/vix
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Vix.cpp
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
  class Config
  {
  public:
    explicit Config(const std::filesystem::path &configPath = "");

    Config(const Config &) = delete;
    Config &operator=(const Config &) = delete;

    static Config &getInstance(const std::filesystem::path &configPath = "");

    void loadConfig();

#if VIX_CORE_WITH_MYSQL
    namespace sql
    {
      class Connection;
    }
    std::shared_ptr<sql::Connection> getDbConnection();
#endif

    std::string getDbPasswordFromEnv();
    const std::string &getDbHost() const noexcept;
    const std::string &getDbUser() const noexcept;
    const std::string &getDbName() const noexcept;
    int getDbPort() const noexcept;
    int getServerPort() const noexcept;
    int getRequestTimeout() const noexcept;
    void setServerPort(int port);
    bool has(const std::string &dottedKey) const noexcept;
    int getInt(const std::string &dottedKey, int defaultValue) const noexcept;
    bool getBool(const std::string &dottedKey, bool defaultValue) const noexcept;
    std::string getString(
        const std::string &dottedKey,
        const std::string &defaultValue) const noexcept;
    int getIOThreads() const noexcept; // 0 => auto
    bool isBenchMode() const noexcept;
    bool getLogAsync() const noexcept;
    int getLogQueueMax() const noexcept;
    bool getLogDropOnOverflow() const noexcept;
    const std::string &getWafMode() const noexcept; // "off"|"basic"|"strict"
    int getWafMaxTargetLen() const noexcept;
    int getWafMaxBodyBytes() const noexcept;
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
}

#endif // VIX_CONFIG_HPP
