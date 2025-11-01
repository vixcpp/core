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

/**
 * @file Config.cpp
 * @brief Implementation notes for vix::Config (maintainers-focused docs).
 *
 * Responsibilities and design choices:
 *  - Resolve a configuration file path with sensible defaults and log each probe.
 *  - Load a JSON document with a conservative schema and default fallbacks.
 *  - Provide noexcept getters for hot-path usage after initialization.
 *  - Optionally construct a MySQL connection (behind VIX_CORE_WITH_MYSQL).
 *  - Support environment override for sensitive values (DB password).
 *
 * Path discovery strategy
 *  - If a path is provided to the constructor, prefer it.
 *  - Otherwise, probe a repo-relative default: <repo_root>/config/config.json
 *    (walks up from this source file's directory).
 *
 * Error handling
 *  - Missing config file: warning + fall back to built-in defaults.
 *  - JSON parse errors / IO failures: escalate via Logger::throwError (terminates or throws).
 *  - Out-of-range server port in setServerPort(): throw via Logger.
 *
 * Thread-safety
 *  - The object is expected to be fully configured before concurrent access.
 *  - Getters are noexcept and read-only.
 */

namespace vix::config
{
    namespace fs = std::filesystem;
    using json = nlohmann::json;
    using Logger = vix::utils::Logger;

    /**
     * @brief Construct and attempt to locate a config file; load if found.
     */
    Config::Config(const fs::path &configPath)
        : configPath_(configPath),
          db_host(DEFAULT_DB_HOST), db_user(DEFAULT_DB_USER), db_pass(DEFAULT_DB_PASS),
          db_name(DEFAULT_DB_NAME), db_port(DEFAULT_DB_PORT),
          server_port(DEFAULT_SERVER_PORT), request_timeout(DEFAULT_REQUEST_TIMEOUT)
    {
        auto &log = vix::utils::Logger::getInstance();
        std::vector<fs::path> candidate_paths;

        if (!configPath.empty())
        {
            candidate_paths.push_back(configPath);
        }
        else
        {
            // <repo_root>/config/config.json
            candidate_paths.push_back(
                fs::path(__FILE__).parent_path() // .../modules/core/src/config
                    .parent_path()               // .../modules/core/src
                    .parent_path()               // .../modules/core
                    .parent_path()               // .../modules
                    .parent_path() /
                "config/config.json");
        }

        bool found = false;
        for (const auto &p : candidate_paths)
        {
            if (fs::exists(p))
            {
                configPath_ = p;
                log.log(Logger::Level::INFO, "Using configuration file: {}", p.string());
                found = true;
                break;
            }
            else
            {
                log.log(Logger::Level::WARN, "Config file not found at: {}", p.string());
            }
        }

        if (!found)
        {
            log.log(Logger::Level::WARN, "No config file found. Using default settings.");
            return;
        }

        loadConfig();
    }

    /**
     * @brief Singleton accessor; first call fixes the path.
     */
    Config &Config::getInstance(const fs::path &configPath)
    {
        static Config instance(configPath);
        return instance;
    }

    /**
     * @brief Load configuration JSON and apply defaults for missing keys.
     */
    void Config::loadConfig()
    {
        auto &log = Logger::getInstance();

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
        }

        log.log(Logger::Level::INFO, "Config loaded from {}", configPath_.string());
    }

    /**
     * @brief Prefer DB password from environment when available.
     */
    std::string Config::getDbPasswordFromEnv()
    {
        auto &log = Logger::getInstance();
        if (const char *password = std::getenv("DB_PASSWORD"))
        {
            log.log(Logger::Level::INFO, "Using DB_PASSWORD from environment.");
            return password;
        }
        log.log(Logger::Level::WARN, "No DB_PASSWORD found in environment; using config/default password.");
        return db_pass;
    }

#if VIX_CORE_WITH_MYSQL
    /**
     * @brief Establish a new MySQL connection using current config values.
     *
     * Notes:
     *  - Uses driver instance from Connector/C++. Exceptions may propagate.
     *  - If `db_name` is set, selects schema immediately after connect.
     */
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

        log.log(Logger::Level::INFO, "Database connection established (host={}, db={}).",
                host, db_name);
        return con;
    }
#endif // VIX_CORE_WITH_MYSQL

    // --- Getters (definitions) ---------------------------------------------------
    const std::string &Config::getDbHost() const noexcept { return db_host; }
    const std::string &Config::getDbUser() const noexcept { return db_user; }
    const std::string &Config::getDbName() const noexcept { return db_name; }
    int Config::getDbPort() const noexcept { return db_port; }
    int Config::getServerPort() const noexcept { return server_port; }
    int Config::getRequestTimeout() const noexcept { return request_timeout; }

    /**
     * @brief Validate and set the HTTP server port at runtime.
     */
    void Config::setServerPort(int port)
    {
        auto &log = Logger::getInstance();
        if (port < 1024 || port > 65535)
            log.throwError("Server port out of range (1024-65535).");
        server_port = port;
        log.log(Logger::Level::INFO, "Server port set to {}", std::to_string(port));
    }
}
