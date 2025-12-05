#ifndef VIX_CONFIG_HPP
#define VIX_CONFIG_HPP

/**
 * @file Config.hpp
 * @brief Application-wide configuration loader and accessor for Vix.cpp.
 *
 * @details
 * `vix::Config` centralizes runtime configuration for the HTTP server and
 * optional subsystems (e.g., SQL). It supports:
 *  - Loading from a JSON file on disk (path provided at construction).
 *  - Reasonable **defaults** for all fields when values are missing.
 *  - An optional `getInstance()` singleton accessor for convenience in apps
 *    that prefer a global configuration handle.
 *
 * ### Typical usage
 * @code{.cpp}
 * using namespace vix;
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

namespace vix::config
{
    /**
     * @class Config
     * @brief Mutable configuration object with JSON file backing.
     */
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

        // --- Accessors (noexcept) -------------------------------------------------
        const std::string &getDbHost() const noexcept;
        const std::string &getDbUser() const noexcept;
        const std::string &getDbName() const noexcept;
        int getDbPort() const noexcept;
        int getServerPort() const noexcept;
        int getRequestTimeout() const noexcept;

        void setServerPort(int port);

        // --- Generic dotted-path helpers (for advanced modules) -------------
        bool has(const std::string &dottedKey) const noexcept;
        int getInt(const std::string &dottedKey, int defaultValue) const noexcept;
        bool getBool(const std::string &dottedKey, bool defaultValue) const noexcept;
        std::string getString(const std::string &dottedKey,
                              const std::string &defaultValue) const noexcept;

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

        // Raw JSON document as loaded from disk (used by generic getters).
        nlohmann::json rawConfig_;

        // Helper: navigate a dotted path like "websocket.max_message_size"
        // inside rawConfig_ and return a node or nullptr if missing.
        const nlohmann::json *findNode(const std::string &dottedKey) const noexcept;
    };
}

#endif // VIX_CONFIG_HPP
