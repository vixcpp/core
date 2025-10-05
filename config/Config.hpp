#ifndef VIX_CONFIG_HPP
#define VIX_CONFIG_HPP

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <memory>
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace Vix
{
    class Config
    {
    public:
        explicit Config(const std::filesystem::path &configPath = "");

        Config(const Config &) = delete;
        Config &operator=(const Config &) = delete;

        static Config &getInstance(const std::filesystem::path &configPath = "");

        void loadConfig();

        std::shared_ptr<sql::Connection> getDbConnection();
        std::string getDbPasswordFromEnv();

        const std::string &getDbHost() const;
        const std::string &getDbUser() const;
        const std::string &getDbName() const;
        int getDbPort() const;
        int getServerPort() const;
        int getRequestTimeout() const;
        void setServerPort(int port);

    private:
        static constexpr const char *DEFAULT_DB_HOST = "localhost";
        static constexpr const char *DEFAULT_DB_USER = "root";
        static constexpr const char *DEFAULT_DB_PASS = "";
        static constexpr const char *DEFAULT_DB_NAME = "";
        static constexpr int DEFAULT_DB_PORT = 3306;
        static constexpr int DEFAULT_SERVER_PORT = 8080;
        static constexpr int DEFAULT_REQUEST_TIMEOUT = 2000; // ✅ nouveau

        std::filesystem::path configPath_;
        std::string db_host;
        std::string db_user;
        std::string db_pass;
        std::string db_name;
        int db_port;
        int server_port;
        int request_timeout; // ✅ nouveau
    };
}

#endif
