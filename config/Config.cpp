#include "Config.hpp"
#include "../utils/Logger.hpp"
#include <fstream>
#include <cstdlib>
#include <spdlog/spdlog.h>

namespace Vix
{
    namespace fs = std::filesystem;
    using json = nlohmann::json;

    Config::Config(const fs::path &configPath)
        : db_host(DEFAULT_DB_HOST),
          db_user(DEFAULT_DB_USER),
          db_pass(DEFAULT_DB_PASS),
          db_name(DEFAULT_DB_NAME),
          db_port(DEFAULT_DB_PORT),
          server_port(DEFAULT_SERVER_PORT)
    {
        std::vector<fs::path> candidates;
        if (!configPath.empty())
        {
            candidates.push_back(configPath);
        }
        else
        {
            fs::path cwd = fs::current_path();

            candidates.push_back(cwd / "vix/config/config.json");
            candidates.push_back(cwd.parent_path() / "vix/config/config.json");
            candidates.push_back(cwd / "../vix/config/config.json");
            candidates.push_back(cwd / "config/config.json");
        }

        bool found = false;
        for (auto &p : candidates)
        {
            if (fs::exists(p))
            {
                configPath_ = fs::canonical(p);
                found = true;
                break;
            }
        }

        if (!found)
        {
            spdlog::warn("Config file not found in any candidate path. Using default settings.");
            configPath_ = "";
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
        auto &log = Vix::Logger::getInstance();

        if (configPath_.empty() || !fs::exists(configPath_))
        {
            log.log(Logger::Level::WARN, "No config file found. Using default settings.");
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

        if (!cfg.contains("database") || !cfg["database"].contains("default"))
            log.throwError("Invalid config file: missing 'database.default' section");

        const auto &db = cfg["database"]["default"];
        db_host = db.value("host", DEFAULT_DB_HOST);
        db_user = db.value("user", DEFAULT_DB_USER);
        db_pass = db.value("password", DEFAULT_DB_PASS);
        db_name = db.value("name", DEFAULT_DB_NAME);
        db_port = db.value("port", DEFAULT_DB_PORT);

        if (cfg.contains("server"))
            server_port = cfg["server"].value("port", DEFAULT_SERVER_PORT);

        log.log(Vix::Logger::Level::INFO, "Config loaded from {}", configPath_.string());
    }

    std::string Config::getDbPasswordFromEnv()
    {
        auto &log = Vix::Logger::getInstance();
        if (const char *password = std::getenv("DB_PASSWORD"))
        {
            log.log(Logger::Level::INFO, "Using DB_PASSWORD from environment.");
            return password;
        }
        log.log(Logger::Level::WARN, "No DB_PASSWORD found in environment, using default password.");
        return db_pass;
    }

    std::shared_ptr<sql::Connection> Config::getDbConnection()
    {
        auto &log = Vix::Logger::getInstance();
        sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
        auto con = std::shared_ptr<sql::Connection>(
            driver->connect("tcp://" + db_host + ":" + std::to_string(db_port), db_user, db_pass));
        con->setSchema(db_name);
        log.log(Logger::Level::INFO, "Database connection established.");
        return con;
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
            log.throwError("Server port out of range (1024-65535).");
        server_port = port;
        log.log(Logger::Level::INFO, "Server port set to {}", std::to_string(port));
    }
}
