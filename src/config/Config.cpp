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

#include <vix/env/EnvFileOptions.hpp>
#include <vix/env/Get.hpp>
#include <vix/env/GetBool.hpp>
#include <vix/env/GetInt.hpp>
#include <vix/env/LoadIntoProcess.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>
#include <iostream>
#include <unordered_map>
#include <unordered_set>

namespace vix::config
{
  namespace
  {
    [[nodiscard]] std::string trim_copy(std::string value)
    {
      auto not_space = [](unsigned char ch)
      { return !std::isspace(ch); };

      value.erase(
          value.begin(),
          std::find_if(value.begin(), value.end(), not_space));

      value.erase(
          std::find_if(value.rbegin(), value.rend(), not_space).base(),
          value.end());

      return value;
    }

    [[nodiscard]] std::string to_upper_ascii(std::string value)
    {
      std::transform(
          value.begin(),
          value.end(),
          value.begin(),
          [](unsigned char c)
          { return static_cast<char>(std::toupper(c)); });

      return value;
    }

    [[nodiscard]] std::string normalize_env_name(std::string value)
    {
      value = trim_copy(std::move(value));

      if (value.empty() || value == ".env")
      {
        return {};
      }

      if (value.rfind(".env.", 0) == 0)
      {
        value.erase(0, 5);
      }

      return value;
    }

    [[nodiscard]] std::string dotted_to_env_key(const std::string &dottedKey)
    {
      std::string out;
      out.reserve(dottedKey.size());

      for (const char c : dottedKey)
      {
        if (c == '.')
        {
          out.push_back('_');
        }
        else
        {
          out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
      }

      return out;
    }

    [[nodiscard]] std::string get_env_string(
        const std::string &envKey,
        const std::string &defaultValue)
    {
      auto value = vix::env::get(envKey);
      if (!value)
      {
        return defaultValue;
      }

      return value.value();
    }

    [[nodiscard]] int get_env_int(
        const std::string &envKey,
        int defaultValue)
    {
      auto value = vix::env::get_int(envKey);
      if (!value)
      {
        return defaultValue;
      }

      return value.value();
    }

    [[nodiscard]] bool get_env_bool(
        const std::string &envKey,
        bool defaultValue)
    {
      auto value = vix::env::get_bool(envKey);
      if (!value)
      {
        return defaultValue;
      }

      return value.value();
    }

    static int get_env_int_with_warning(
        const std::string &envKey,
        int defaultValue)
    {
      static std::unordered_set<std::string> warned;

      auto value = vix::env::get_int(envKey);

      if (!value)
      {
        auto silent = vix::env::get_bool("VIX_ENV_SILENT");
        const bool isSilent = silent ? silent.value() : false;

        if (!isSilent && warned.insert(envKey).second)
        {
          std::cerr << "i " << envKey
                    << " not set. Using default: "
                    << defaultValue << "\n";
        }

        return defaultValue;
      }

      return value.value();
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
        bench_mode_(DEFAULT_BENCH_MODE),
        tls_enabled_(DEFAULT_TLS_ENABLED),
        tls_cert_file_(DEFAULT_TLS_CERT_FILE),
        tls_key_file_(DEFAULT_TLS_KEY_FILE)
  {
    if (configPath_.empty())
    {
      configPath_ = ".env";
    }

    loadConfig();
  }

  void Config::loadConfig()
  {
    rawConfig_ = nlohmann::json::object();

    vix::env::EnvFileOptions env_options{};
    env_options.base_dir = configPath_.has_parent_path()
                               ? configPath_.parent_path().string()
                               : ".";
    env_options.filename = ".env";
    env_options.mode = vix::env::EnvFileMode::Layered;
    env_options.load_base_file = true;
    env_options.load_local_file = true;
    env_options.ignore_missing_files = true;

    const std::string config_filename = configPath_.filename().string();
    const std::string environment_name = normalize_env_name(config_filename);

    if (!environment_name.empty())
    {
      env_options.environment_name = environment_name;
      env_options.load_environment_file = true;
      env_options.load_environment_local_file = true;
    }

    if (auto err = vix::env::load_layered_into_process(env_options); err)
    {
      throw std::runtime_error(
          "Failed to load environment configuration: " +
          std::string(err.message()));
    }

    db_host = get_env_string("DATABASE_DEFAULT_HOST", DEFAULT_DB_HOST);
    db_user = get_env_string("DATABASE_DEFAULT_USER", DEFAULT_DB_USER);
    db_name = get_env_string("DATABASE_DEFAULT_NAME", DEFAULT_DB_NAME);
    db_port = get_env_int("DATABASE_DEFAULT_PORT", DEFAULT_DB_PORT);

    db_pass = getDbPasswordFromEnv();

    server_port = get_env_int_with_warning("SERVER_PORT", DEFAULT_SERVER_PORT);

    request_timeout = get_env_int("SERVER_REQUEST_TIMEOUT", DEFAULT_REQUEST_TIMEOUT);
    io_threads_ = get_env_int("SERVER_IO_THREADS", DEFAULT_IO_THREADS);
    session_timeout_sec_ =
        get_env_int("SERVER_SESSION_TIMEOUT_SEC", DEFAULT_SESSION_TIMEOUT_SEC);
    bench_mode_ = get_env_bool("SERVER_BENCH_MODE", DEFAULT_BENCH_MODE);

    tls_enabled_ = get_env_bool("SERVER_TLS_ENABLED", DEFAULT_TLS_ENABLED);
    tls_cert_file_ = get_env_string("SERVER_TLS_CERT_FILE", DEFAULT_TLS_CERT_FILE);
    tls_key_file_ = get_env_string("SERVER_TLS_KEY_FILE", DEFAULT_TLS_KEY_FILE);

    log_async_ = get_env_bool("LOGGING_ASYNC", DEFAULT_LOG_ASYNC);
    log_queue_max_ = get_env_int("LOGGING_QUEUE_MAX", DEFAULT_LOG_QUEUE_MAX);
    log_drop_on_overflow_ =
        get_env_bool("LOGGING_DROP_ON_OVERFLOW", DEFAULT_LOG_DROP_ON_OVERFLOW);

    waf_mode_ = get_env_string("WAF_MODE", DEFAULT_WAF_MODE);
    waf_max_target_len_ =
        get_env_int("WAF_MAX_TARGET_LEN", DEFAULT_WAF_MAX_TARGET_LEN);
    waf_max_body_bytes_ =
        get_env_int("WAF_MAX_BODY_BYTES", DEFAULT_WAF_MAX_BODY_BYTES);
  }

#if VIX_CORE_WITH_MYSQL
  std::shared_ptr<sql::Connection> Config::getDbConnection()
  {
    return {};
  }
#endif

  std::string Config::getDbPasswordFromEnv() const
  {
    if (auto value = vix::env::get("VIX_DB_PASSWORD"); value && !value.value().empty())
    {
      return value.value();
    }

    if (auto value = vix::env::get("DATABASE_DEFAULT_PASSWORD"); value && !value.value().empty())
    {
      return value.value();
    }

    if (auto value = vix::env::get("DB_PASSWORD"); value && !value.value().empty())
    {
      return value.value();
    }

    if (auto value = vix::env::get("MYSQL_PASSWORD"); value && !value.value().empty())
    {
      return value.value();
    }

    return DEFAULT_DB_PASS;
  }

  void Config::set(const std::string &dottedKey, const nlohmann::json &value)
  {
    if (dottedKey.empty())
    {
      return;
    }

    nlohmann::json *node = &rawConfig_;
    std::size_t start = 0;

    while (start < dottedKey.size())
    {
      const std::size_t dot = dottedKey.find('.', start);
      const std::string key =
          dottedKey.substr(start, dot == std::string::npos ? std::string::npos : dot - start);

      if (dot == std::string::npos)
      {
        (*node)[key] = value;
        return;
      }

      node = &((*node)[key]);
      start = dot + 1;
    }
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
    set("server.port", port);
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
    if (findNode(dottedKey) != nullptr)
    {
      return true;
    }

    auto env_value = vix::env::get(dotted_to_env_key(dottedKey));
    return static_cast<bool>(env_value);
  }

  int Config::getInt(const std::string &dottedKey, int defaultValue) const noexcept
  {
    const nlohmann::json *node = findNode(dottedKey);
    if (node)
    {
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

      if (node->is_boolean())
      {
        return node->get<bool>() ? 1 : 0;
      }

      if (node->is_string())
      {
        try
        {
          const std::string &value = node->get_ref<const std::string &>();
          std::size_t pos = 0;
          const int parsed = std::stoi(value, &pos);
          if (pos == value.size())
          {
            return parsed;
          }
        }
        catch (...)
        {
        }
      }
    }

    return get_env_int(dotted_to_env_key(dottedKey), defaultValue);
  }

  bool Config::getBool(const std::string &dottedKey, bool defaultValue) const noexcept
  {
    const nlohmann::json *node = findNode(dottedKey);
    if (node)
    {
      if (node->is_boolean())
      {
        return node->get<bool>();
      }

      if (node->is_number_integer())
      {
        return node->get<int>() != 0;
      }

      if (node->is_number_unsigned())
      {
        return node->get<unsigned int>() != 0U;
      }

      if (node->is_string())
      {
        const std::string value = to_upper_ascii(node->get<std::string>());

        if (value == "1" || value == "TRUE" || value == "YES" || value == "ON")
        {
          return true;
        }

        if (value == "0" || value == "FALSE" || value == "NO" || value == "OFF")
        {
          return false;
        }
      }
    }

    return get_env_bool(dotted_to_env_key(dottedKey), defaultValue);
  }

  std::string Config::getString(
      const std::string &dottedKey,
      const std::string &defaultValue) const noexcept
  {
    const nlohmann::json *node = findNode(dottedKey);
    if (node)
    {
      if (node->is_string())
      {
        return node->get<std::string>();
      }

      if (node->is_boolean())
      {
        return node->get<bool>() ? "true" : "false";
      }

      if (node->is_number_integer())
      {
        return std::to_string(node->get<int>());
      }

      if (node->is_number_unsigned())
      {
        return std::to_string(node->get<unsigned int>());
      }

      if (node->is_number_float())
      {
        return std::to_string(node->get<double>());
      }
    }

    return get_env_string(dotted_to_env_key(dottedKey), defaultValue);
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

  bool Config::isTlsEnabled() const noexcept
  {
    return tls_enabled_;
  }

  const std::string &Config::getTlsCertFile() const noexcept
  {
    return tls_cert_file_;
  }

  const std::string &Config::getTlsKeyFile() const noexcept
  {
    return tls_key_file_;
  }

  vix::server::TlsConfig Config::getTlsConfig() const
  {
    vix::server::TlsConfig cfg{};
    cfg.enabled = tls_enabled_;
    cfg.cert_file = tls_cert_file_;
    cfg.key_file = tls_key_file_;
    return cfg;
  }

} // namespace vix::config
