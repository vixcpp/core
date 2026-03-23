/**
 *
 * @file Config.cpp
 * @author Gaspard Kirira
 *
 * Copyright 2025, Gaspard Kirira. All rights reserved.
 * https://github.com/vixcpp/vix
 * Use of this source code is governed by a MIT license that can be found in the License file.
 *
 * Vix.cpp
 *
 */

#include <vix/config/Config.hpp>

#include <cstdlib>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace vix::config
{
  namespace
  {
    nlohmann::json make_default_config()
    {
      return {
          {"server",
           {
               {"port", Config::getInstance().getServerPort()},
           }}};
    }
  } // namespace

  Config::Config(const std::filesystem::path &configPath)
      : configPath_(configPath),
        db_host(DEFAULT_DB_HOST),
        db_user(DEFAULT_DB_USER),
        db_pass(DEFAULT_DB_PASS),
        db_name(DEFAULT_DB_NAME),
        db_port(DEFAULT_DB_PORT),
        server_port(DEFAULT_SERVER_PORT),
        request_timeout(DEFAULT_REQUEST_TIMEOUT),
        rawConfig_(nlohmann::json::object()),
        io_threads_(DEFAULT_IO_THREADS),
        log_async_(DEFAULT_LOG_ASYNC),
        log_queue_max_(DEFAULT_LOG_QUEUE_MAX),
        log_drop_on_overflow_(DEFAULT_LOG_DROP_ON_OVERFLOW),
        waf_mode_(DEFAULT_WAF_MODE),
        waf_max_target_len_(DEFAULT_WAF_MAX_TARGET_LEN),
        waf_max_body_bytes_(DEFAULT_WAF_MAX_BODY_BYTES),
        session_timeout_sec_(DEFAULT_SESSION_TIMEOUT_SEC),
        bench_mode_(DEFAULT_BENCH_MODE)
  {
    if (configPath_.empty())
    {
      configPath_ = "config.json";
    }

    loadConfig();
  }

  Config &Config::getInstance(const std::filesystem::path &configPath)
  {
    static Config instance(configPath);
    return instance;
  }

  void Config::loadConfig()
  {
    rawConfig_ = nlohmann::json::object();

    if (!configPath_.empty() && std::filesystem::exists(configPath_))
    {
      std::ifstream file(configPath_);
      if (!file.is_open())
      {
        throw std::runtime_error("Failed to open config file: " + configPath_.string());
      }

      try
      {
        file >> rawConfig_;
      }
      catch (const std::exception &e)
      {
        throw std::runtime_error(
            "Failed to parse config file '" + configPath_.string() + "': " + e.what());
      }
    }

    db_host = getString("database.default.host", DEFAULT_DB_HOST);
    db_user = getString("database.default.user", DEFAULT_DB_USER);
    db_name = getString("database.default.name", DEFAULT_DB_NAME);
    db_port = getInt("database.default.port", DEFAULT_DB_PORT);

    {
      const std::string password_from_config =
          getString("database.default.password", DEFAULT_DB_PASS);

      if (!password_from_config.empty())
      {
        db_pass = password_from_config;
      }
      else
      {
        db_pass = getDbPasswordFromEnv();
      }
    }

    server_port = getInt("server.port", DEFAULT_SERVER_PORT);
    request_timeout = getInt("server.request_timeout", DEFAULT_REQUEST_TIMEOUT);
    io_threads_ = getInt("server.io_threads", DEFAULT_IO_THREADS);
    session_timeout_sec_ =
        getInt("server.session_timeout_sec", DEFAULT_SESSION_TIMEOUT_SEC);
    bench_mode_ = getBool("server.bench_mode", DEFAULT_BENCH_MODE);

    log_async_ = getBool("logging.async", DEFAULT_LOG_ASYNC);
    log_queue_max_ = getInt("logging.queue_max", DEFAULT_LOG_QUEUE_MAX);
    log_drop_on_overflow_ =
        getBool("logging.drop_on_overflow", DEFAULT_LOG_DROP_ON_OVERFLOW);

    waf_mode_ = getString("waf.mode", DEFAULT_WAF_MODE);
    waf_max_target_len_ =
        getInt("waf.max_target_len", DEFAULT_WAF_MAX_TARGET_LEN);
    waf_max_body_bytes_ =
        getInt("waf.max_body_bytes", DEFAULT_WAF_MAX_BODY_BYTES);
  }

#if VIX_CORE_WITH_MYSQL
  std::shared_ptr<sql::Connection> Config::getDbConnection()
  {
    return {};
  }
#endif

  std::string Config::getDbPasswordFromEnv()
  {
    if (const char *value = std::getenv("VIX_DB_PASSWORD"); value && *value != '\0')
    {
      return std::string(value);
    }

    if (const char *value = std::getenv("DB_PASSWORD"); value && *value != '\0')
    {
      return std::string(value);
    }

    if (const char *value = std::getenv("MYSQL_PASSWORD"); value && *value != '\0')
    {
      return std::string(value);
    }

    return DEFAULT_DB_PASS;
  }

  const std::string &Config::getDbHost() const noexcept
  {
    return db_host;
  }

  const std::string &Config::getDbUser() const noexcept
  {
    return db_user;
  }

  const std::string &Config::getDbName() const noexcept
  {
    return db_name;
  }

  int Config::getDbPort() const noexcept
  {
    return db_port;
  }

  int Config::getServerPort() const noexcept
  {
    return server_port;
  }

  int Config::getRequestTimeout() const noexcept
  {
    return request_timeout;
  }

  void Config::setServerPort(int port)
  {
    server_port = port;
  }

  const nlohmann::json *Config::findNode(const std::string &dottedKey) const noexcept
  {
    if (dottedKey.empty())
    {
      return nullptr;
    }

    const nlohmann::json *node = &rawConfig_;
    std::size_t start = 0;

    while (start < dottedKey.size())
    {
      const std::size_t dot = dottedKey.find('.', start);
      const std::string key =
          dottedKey.substr(start, dot == std::string::npos ? std::string::npos : dot - start);

      if (!node->is_object())
      {
        return nullptr;
      }

      auto it = node->find(key);
      if (it == node->end())
      {
        return nullptr;
      }

      node = &(*it);

      if (dot == std::string::npos)
      {
        break;
      }

      start = dot + 1;
    }

    return node;
  }

  bool Config::has(const std::string &dottedKey) const noexcept
  {
    return findNode(dottedKey) != nullptr;
  }

  int Config::getInt(const std::string &dottedKey, int defaultValue) const noexcept
  {
    const nlohmann::json *node = findNode(dottedKey);
    if (!node)
    {
      return defaultValue;
    }

    if (node->is_number_integer())
    {
      return node->get<int>();
    }

    if (node->is_number_unsigned())
    {
      return static_cast<int>(node->get<unsigned int>());
    }

    if (node->is_number_float())
    {
      return static_cast<int>(node->get<double>());
    }

    return defaultValue;
  }

  bool Config::getBool(const std::string &dottedKey, bool defaultValue) const noexcept
  {
    const nlohmann::json *node = findNode(dottedKey);
    if (!node)
    {
      return defaultValue;
    }

    if (node->is_boolean())
    {
      return node->get<bool>();
    }

    return defaultValue;
  }

  std::string Config::getString(
      const std::string &dottedKey,
      const std::string &defaultValue) const noexcept
  {
    const nlohmann::json *node = findNode(dottedKey);
    if (!node)
    {
      return defaultValue;
    }

    if (node->is_string())
    {
      return node->get<std::string>();
    }

    return defaultValue;
  }

  int Config::getIOThreads() const noexcept
  {
    return io_threads_;
  }

  bool Config::isBenchMode() const noexcept
  {
#ifdef VIX_BENCH_MODE
    return true;
#else
    return bench_mode_;
#endif
  }

  bool Config::getLogAsync() const noexcept
  {
    return log_async_;
  }

  int Config::getLogQueueMax() const noexcept
  {
    return log_queue_max_;
  }

  bool Config::getLogDropOnOverflow() const noexcept
  {
    return log_drop_on_overflow_;
  }

  const std::string &Config::getWafMode() const noexcept
  {
    return waf_mode_;
  }

  int Config::getWafMaxTargetLen() const noexcept
  {
    return waf_max_target_len_;
  }

  int Config::getWafMaxBodyBytes() const noexcept
  {
    return waf_max_body_bytes_;
  }

  int Config::getSessionTimeoutSec() const noexcept
  {
    return session_timeout_sec_;
  }

} // namespace vix::config
