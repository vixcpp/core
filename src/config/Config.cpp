/**
 *
 *  @file Config.cpp
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
#include <vix/config/Config.hpp>
#include <vix/utils/Logger.hpp>

#if VIX_CORE_WITH_MYSQL
#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#endif

#include <fstream>
#include <cstdlib>
#include <vector>
#include <sstream>

namespace vix::config
{
  namespace fs = std::filesystem;
  using json = nlohmann::json;
  using Logger = vix::utils::Logger;

  Config::Config(const fs::path &configPath)
      : configPath_(configPath),
        db_host(DEFAULT_DB_HOST), db_user(DEFAULT_DB_USER), db_pass(DEFAULT_DB_PASS),
        db_name(DEFAULT_DB_NAME), db_port(DEFAULT_DB_PORT),
        server_port(DEFAULT_SERVER_PORT), request_timeout(DEFAULT_REQUEST_TIMEOUT),
        rawConfig_(nlohmann::json::object()), io_threads_(DEFAULT_IO_THREADS), log_async_(DEFAULT_LOG_ASYNC), log_queue_max_(DEFAULT_LOG_QUEUE_MAX), log_drop_on_overflow_(DEFAULT_LOG_DROP_ON_OVERFLOW), waf_mode_(DEFAULT_WAF_MODE), waf_max_target_len_(DEFAULT_WAF_MAX_TARGET_LEN), waf_max_body_bytes_(DEFAULT_WAF_MAX_BODY_BYTES)
  {
    auto &log = vix::utils::Logger::getInstance();
    std::vector<fs::path> candidate_paths;

    if (!configPath.empty())
    {
      if (configPath.is_absolute())
      {
        candidate_paths.push_back(configPath);
      }
      else
      {
        candidate_paths.push_back(fs::current_path() / configPath);
        auto parent = fs::current_path().parent_path();
        if (!parent.empty())
        {
          candidate_paths.push_back(parent / configPath);
        }
      }
    }
    else
    {
      candidate_paths.push_back(
          fs::path(__FILE__).parent_path() // .../modules/core/src/config
              .parent_path()               // .../modules/core/src
              .parent_path()               // .../modules/core
              .parent_path()               // .../modules
              .parent_path() /             // .../
          "config/config.json");
    }

    bool found = false;
    for (const auto &p : candidate_paths)
    {
      if (fs::exists(p))
      {
        configPath_ = p;
        found = true;
        break;
      }
    }

    if (!found)
    {
      log.log(Logger::Level::DEBUG, "No config file found. Using default settings.");
      return;
    }

    loadConfig();
  }

  Config &Config::getInstance(const fs::path &configPath)
  {
    static Config instance(configPath);
    return instance;
  }

  void Config::loadConfig()
  {
    auto &log = Logger::getInstance();

    if (configPath_.empty() || !fs::exists(configPath_))
    {
      log.log(Logger::Level::DEBUG, "No config file found. Using default settings.");
      return;
    }

    std::ifstream file(configPath_, std::ios::in | std::ios::binary);
    if (!file.is_open())
      log.throwError(fmt::format("Unable to open configuration file: {}", configPath_.string()));

    json cfg;
    try
    {
      file >> cfg;
    }
    catch (const json::parse_error &e)
    {
      log.throwError(fmt::format("JSON parsing error in config file: {}", e.what()));
    }

    rawConfig_ = cfg;

    if (cfg.contains("database") && cfg["database"].contains("default"))
    {
      const auto &db = cfg["database"]["default"];
      db_host = db.value("host", DEFAULT_DB_HOST);
      db_user = db.value("user", DEFAULT_DB_USER);
      db_pass = db.value("password", DEFAULT_DB_PASS);
      db_name = db.value("name", DEFAULT_DB_NAME);
      db_port = db.value("port", DEFAULT_DB_PORT);
    }

    if (cfg.contains("server"))
    {
      const auto &server = cfg["server"];
      server_port = server.value("port", DEFAULT_SERVER_PORT);
      request_timeout = server.value("request_timeout", DEFAULT_REQUEST_TIMEOUT);

      // NEW
      io_threads_ = server.value("io_threads", DEFAULT_IO_THREADS);
    }
    else
    {
      // fallback via dotted path (si jamais tu changes de schema)
      io_threads_ = getInt("server.io_threads", DEFAULT_IO_THREADS);
    }

    // NEW: logging
    log_async_ = getBool("logging.async", DEFAULT_LOG_ASYNC);
    log_queue_max_ = getInt("logging.queue_max", DEFAULT_LOG_QUEUE_MAX);
    log_drop_on_overflow_ = getBool("logging.drop_on_overflow", DEFAULT_LOG_DROP_ON_OVERFLOW);

    // NEW: waf
    waf_mode_ = getString("waf.mode", DEFAULT_WAF_MODE);
    waf_max_target_len_ = getInt("waf.max_target_len", DEFAULT_WAF_MAX_TARGET_LEN);
    waf_max_body_bytes_ = getInt("waf.max_body_bytes", DEFAULT_WAF_MAX_BODY_BYTES);
  }

  const nlohmann::json *Config::findNode(const std::string &dottedKey) const noexcept
  {
    if (rawConfig_.is_null())
      return nullptr;

    const json *node = &rawConfig_;
    std::stringstream ss(dottedKey);
    std::string token;

    while (std::getline(ss, token, '.'))
    {
      if (!node->is_object())
        return nullptr;

      auto it = node->find(token);
      if (it == node->end())
        return nullptr;

      node = &(*it);
    }
    return node;
  }

  std::string Config::getDbPasswordFromEnv()
  {
    auto &log = Logger::getInstance();
    if (const char *password = std::getenv("DB_PASSWORD"))
    {
      log.log(Logger::Level::DEBUG, "Using DB_PASSWORD from environment.");
      return password;
    }
    log.log(Logger::Level::DEBUG, "No DB_PASSWORD found in environment; using config/default password.");
    return db_pass;
  }

#if VIX_CORE_WITH_MYSQL
  std::shared_ptr<sql::Connection> Config::getDbConnection()
  {
    auto &log = Logger::getInstance();

    sql::Driver *driver = sql::mysql::get_driver_instance();

    const std::string host = "tcp://" + db_host + ":" + std::to_string(db_port);
    const std::string user = db_user;
    const std::string pass = getDbPasswordFromEnv();

    std::shared_ptr<sql::Connection> con(driver->connect(host, user, pass));
    if (!db_name.empty())
      con->setSchema(db_name);

    log.log(Logger::Level::DEBUG, "Database connection established (host={}, db={}).", host, db_name);
    return con;
  }
#endif // VIX_CORE_WITH_MYSQL

  const std::string &Config::getDbHost() const noexcept { return db_host; }
  const std::string &Config::getDbUser() const noexcept { return db_user; }
  const std::string &Config::getDbName() const noexcept { return db_name; }
  int Config::getDbPort() const noexcept { return db_port; }
  int Config::getServerPort() const noexcept { return server_port; }
  int Config::getRequestTimeout() const noexcept { return request_timeout; }

  void Config::setServerPort(int port)
  {
    auto &log = Logger::getInstance();
    if (port < 1024 || port > 65535)
      log.throwError("Server port out of range (1024-65535).");
    server_port = port;
  }

  bool Config::has(const std::string &dottedKey) const noexcept
  {
    return findNode(dottedKey) != nullptr;
  }

  int Config::getInt(const std::string &dottedKey, int defaultValue) const noexcept
  {
    const auto *node = findNode(dottedKey);
    if (!node)
      return defaultValue;

    try
    {
      if (node->is_number_integer())
        return node->get<int>();
      if (node->is_number_float())
        return static_cast<int>(node->get<double>());
      if (node->is_string())
        return std::stoi(node->get<std::string>());
    }
    catch (...)
    {
    }
    return defaultValue;
  }

  bool Config::getBool(const std::string &dottedKey, bool defaultValue) const noexcept
  {
    const auto *node = findNode(dottedKey);
    if (!node)
      return defaultValue;

    try
    {
      if (node->is_boolean())
        return node->get<bool>();
      if (node->is_number_integer())
        return node->get<int>() != 0;
      if (node->is_string())
      {
        auto s = node->get<std::string>();
        if (s == "true" || s == "1" || s == "on" || s == "yes")
          return true;
        if (s == "false" || s == "0" || s == "off" || s == "no")
          return false;
      }
    }
    catch (...)
    {
    }
    return defaultValue;
  }

  std::string Config::getString(const std::string &dottedKey,
                                const std::string &defaultValue) const noexcept
  {
    const auto *node = findNode(dottedKey);
    if (!node)
      return defaultValue;

    try
    {
      if (node->is_string())
        return node->get<std::string>();
      return node->dump();
    }
    catch (...)
    {
      return defaultValue;
    }
  }

  // --- Prod-safe server knobs -----------------------------------------
  int Config::getIOThreads() const noexcept { return io_threads_; }

  bool Config::isBenchMode() const noexcept
  {
#ifdef VIX_BENCH_MODE
    return true;
#else
    return false;
#endif
  }

  // --- Logging knobs ---------------------------------------------------
  bool Config::getLogAsync() const noexcept { return log_async_; }
  int Config::getLogQueueMax() const noexcept { return log_queue_max_; }
  bool Config::getLogDropOnOverflow() const noexcept { return log_drop_on_overflow_; }

  // --- WAF knobs -------------------------------------------------------
  const std::string &Config::getWafMode() const noexcept { return waf_mode_; }
  int Config::getWafMaxTargetLen() const noexcept { return waf_max_target_len_; }
  int Config::getWafMaxBodyBytes() const noexcept { return waf_max_body_bytes_; }

}
