#include "Config.hpp"
#include "../utils/Logger.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#include <spdlog/spdlog.h>

namespace Vix
{
    namespace fs = std::filesystem;
    using json = nlohmann::json;

    const fs::path Config::DEFAULT_CONFIG_PATH = fs::absolute(fs::path(__FILE__).parent_path().parent_path() / "/config/config.json");

    Config::Config()
        : db_host(DEFAULT_DB_HOST),
          db_user(DEFAULT_DB_USER),
          db_pass(DEFAULT_DB_PASS),
          db_name(DEFAULT_DB_NAME),
          db_port(DEFAULT_DB_PORT),
          server_port(DEFAULT_SERVER_PORT)
    {
    }

    void Config::loadConfig()
    {
        auto &log = Vix::Logger::getInstance();
        const fs::path configPath = std::getenv("VIX_CONFIG") ? std::getenv("VIX_CONFIG") : DEFAULT_CONFIG_PATH;

        if (!fs::exists(configPath))
        {
            log.throwError(fmt::format("Configuration file does not exist: {}", configPath.string()));
        }

        std::ifstream config_file(configPath, std::ios::in | std::ios::binary);
        if (!config_file.is_open())
        {
            log.throwError(fmt::format("Unable to open configuration file: {}", configPath.string()));
        }

        json config;
        try
        {
            config_file >> config;
        }
        catch (const json::parse_error &e)
        {
            log.throwError(fmt::format("Json parsing error in config file: {}", e.what()));
        }

        if (!config.contains("database") || !config["database"].contains("default"))
        {
            log.throwError("Invalid config file: missing 'database.default' section");
        }

        const auto &db = config["database"]["default"];

        try
        {
            db_host = db.value("host", DEFAULT_DB_HOST);
            db_user = db.value("user", DEFAULT_DB_USER);
            db_pass = db.value("password", DEFAULT_DB_PASS);
            db_name = db.value("name", DEFAULT_DB_NAME);
            server_port = db.value("port", DEFAULT_DB_PORT);
        }
        catch (const json::type_error &e)
        {
            log.throwError("Invalid types in config file: " + std::string(e.what()));
        }

        if (!config.contains("server"))
        {
            log.throwError("Invalid config file: missing 'server' section");
        }
        server_port = config["server"].value("port", DEFAULT_SERVER_PORT);

        log.log(Vix::Logger::Level::INFO, "Config loaded from {}", configPath.string());
    }

    std::string Config::getDbPasswordFromEnv()
    {
        auto &log = Vix::Logger::getInstance();
        if (const char *password = std::getenv("DB_PASSWORD"))
        {
            log.log(Logger::Level::INFO, "Using DB_PASSWORD from environment variables.");
            return std::string(password);
        }
        else
        {
            log.log(Logger::Level::WARN, "No DB_PASSWORD found in environment variables, using default password (empty).");
            return db_pass;
        }
    }

    void Config::loadConfigOnce()
    {
        static bool config_loaded = false;
        if (!config_loaded)
        {
            loadConfig();
            config_loaded = true;
        }
    }

    std::shared_ptr<sql::Connection> Config::getDbConnection()
    {
        auto &log = Vix::Logger::getInstance();
        try
        {
            sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
            if (!driver)
            {
                log.throwError("Failed to get MySQL driver instance.");
            }

            std::shared_ptr<sql::Connection> con(
                driver->connect("tcp://" + db_host + ":" + std::to_string(db_port), db_user, db_pass));
            if (!con)
            {
                log.throwError("Failed to connect to the database.");
            }

            con->setSchema(getDbName());
            log.log(Logger::Level::INFO, "Database connection established successfully.");
            return con;
        }
        catch (const sql::SQLException &e)
        {
            log.throwError(fmt::format("Database connection error: {}", std::string(e.what())));
        }
    }

    const std::string &Config::getDbHost() const { return db_host; }
    const std::string &Config::getDbUser() const { return db_user; }
    const std::string &Config::getDbName() const { return db_name; }
    int Config::getDbPort() const { return db_port; }
    int Config::getServerPort() const { return server_port; }

    void Config::setServerPort(int port)
    {
        auto &log = Vix::Logger::getInstance();
        if (port < 1024 || port > 65535)
        {
            log.throwError("Server port out of range (1024-65535).");
        }
        server_port = port;
        log.log(Logger::Level::INFO, "Server port set to {}", std::to_string(port));
    }

    Config &Config::getInstance()
    {
        static Config instance;
        return instance;
    }
}