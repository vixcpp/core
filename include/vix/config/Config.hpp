#ifndef VIX_CONFIG_HPP
#define VIX_CONFIG_HPP

/**
 * @file Config.hpp
 * @brief Application-wide configuration loader and accessor for Vix.cpp.
 *
 * @details
 * `Vix::Config` centralizes runtime configuration for the HTTP server and
 * optional subsystems (e.g., SQL). It supports:
 *  - Loading from a JSON file on disk (path provided at construction).
 *  - Reasonable **defaults** for all fields when values are missing.
 *  - An optional `getInstance()` singleton accessor for convenience in apps
 *    that prefer a global configuration handle.
 *
 * ### Typical usage
 * @code{.cpp}
 * using namespace Vix;
 * Config cfg{"/etc/vix/config.json"};
 * cfg.loadConfig();
 *
 * HTTPServer server(cfg);
 * server.getRouter()->add_route(http::verb::get, "/health",
 *   std::make_shared<RequestHandler>("/health",
 *     [](const auto& req, ResponseWrapper& res){ res.text("OK"); }));
 * @endcode
 *
 * ### Thread-safety
 * - After initialization (construction + `loadConfig()`), getters are
 *   `noexcept` and can be called concurrently.
 * - Mutating APIs (e.g., `setServerPort`) are minimal; if used concurrently,
 *   callers must provide external synchronization.
 *
 * ### Configuration schema (JSON)
 * The loader is expected to accept keys like:
 * - `db.host` (string)     — database host, default `localhost`
 * - `db.user` (string)     — database user,  default `root`
 * - `db.pass` (string)     — database pass,  default empty
 * - `db.name` (string)     — database name,  default empty
 * - `db.port` (int)        — database port,  default `3306`
 * - `server.port` (int)    — HTTP port,      default `8080`
 * - `server.request_timeout` (int, ms) — per-request timeout, default `2000`
 *
 * ### Environment overrides
 * `getDbPasswordFromEnv()` can be used to fetch sensitive credentials from the
 * environment rather than storing them in the JSON file.
 */

#include <memory>
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace Vix
{
    /**
     * @class Config
     * @brief Mutable configuration object with JSON file backing.
     */
    class Config
    {
    public:
        /**
         * @brief Construct a config with an optional JSON file path.
         * @param configPath Filesystem path to a JSON configuration file.
         *                   If empty, only defaults and environment are used.
         */
        explicit Config(const std::filesystem::path &configPath = "");

        // Non-copyable; prefer a single source of truth.
        Config(const Config &) = delete;
        Config &operator=(const Config &) = delete;

        /**
         * @brief Global accessor to a process-wide configuration instance.
         * @param configPath Optional path used on first call to initialize.
         * @return Singleton Config reference.
         * @note This is a convenience for apps that prefer global access. If
         *       your code favors dependency injection, construct `Config`
         *       directly and pass references explicitly.
         */
        static Config &getInstance(const std::filesystem::path &configPath = "");

        /**
         * @brief Load or reload configuration from `configPath_` if provided.
         *
         * Missing keys fall back to sensible defaults. Environment overrides
         * may be applied for sensitive values (e.g., DB password).
         */
        void loadConfig();

#if VIX_CORE_WITH_MYSQL
        namespace sql
        {
            class Connection;
        }
        /**
         * @brief Build or return a cached SQL connection (if enabled).
         * @return A shared pointer to an active SQL connection.
         */
        std::shared_ptr<sql::Connection> getDbConnection();
#endif

        /**
         * @brief Retrieve the DB password from environment variables.
         * @return The password string (may be empty if not set).
         */
        std::string getDbPasswordFromEnv();

        // --- Accessors (noexcept) -------------------------------------------------
        /** @return Database host (default: "localhost"). */
        const std::string &getDbHost() const noexcept;
        /** @return Database user (default: "root"). */
        const std::string &getDbUser() const noexcept;
        /** @return Database name (default: empty). */
        const std::string &getDbName() const noexcept;
        /** @return Database port (default: 3306). */
        int getDbPort() const noexcept;
        /** @return HTTP server port (default: 8080). */
        int getServerPort() const noexcept;
        /** @return Per-request timeout in ms (default: 2000). */
        int getRequestTimeout() const noexcept;

        /**
         * @brief Override the HTTP server port at runtime.
         * @param port New port number.
         */
        void setServerPort(int port);

    private:
        // --- Defaults -------------------------------------------------------------
        static constexpr const char *DEFAULT_DB_HOST = "localhost";
        static constexpr const char *DEFAULT_DB_USER = "root";
        static constexpr const char *DEFAULT_DB_PASS = "";
        static constexpr const char *DEFAULT_DB_NAME = "";
        static constexpr int DEFAULT_DB_PORT = 3306;
        static constexpr int DEFAULT_SERVER_PORT = 8080;
        static constexpr int DEFAULT_REQUEST_TIMEOUT = 2000; // ms

        // --- Backing storage ------------------------------------------------------
        std::filesystem::path configPath_;
        std::string db_host;
        std::string db_user;
        std::string db_pass;
        std::string db_name;
        int db_port;
        int server_port;
        int request_timeout;
    };
}

#endif // VIX_CONFIG_HPP
