#ifndef VIX_CONFIG_HPP
#define VIX_CONFIG_HPP

#include <mysql_driver.h>
#include <mysql_connection.h>
#include <cppconn/prepared_statement.h>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>
#include <mutex>
#include <stdexcept>
#include <cstdlib>
#include <memory>
#include <filesystem>

namespace Vix
{
    class Config
    {
    public:
        /**
         * @brief Constructor for the Config class.
         *
         * Initializes the class members with default values.
         */
        Config();

        /**
         * @brief Deleted copy constructor.
         *
         * Prevents copying of the Config object.
         */
        Config(const Config &) = delete;

        /**
         * @brief Deleted copy assignment operator.
         *
         * Prevents assignment of one Config object to another.
         */
        Config &operator=(const Config &) = delete;

        /**
         * @brief Accesses the singleton instance of the configuration.
         *
         * @return The unique Config instance.
         */
        static Config &getInstance();

        /**
         * @brief Loads the configuration from a JSON file.
         *
         * Reads and loads configuration data from the `config.json` file.
         * Values are assigned to the class members.
         *
         * @throws std::runtime_error If the file cannot be opened or parsing fails.
         */
        void loadConfig();

        /**
         * @brief Loads the configuration once.
         *
         * Ensures the configuration is loaded only once during the program execution.
         */
        void loadConfigOnce();

        /**
         * @brief Obtains a MySQL database connection.
         *
         * Creates and returns a MySQL connection using the connection details
         * specified in the configuration.
         *
         * @return A valid MySQL connection shared pointer.
         * @throws std::runtime_error If the connection fails.
         */
        std::shared_ptr<sql::Connection> getDbConnection();

        /**
         * @brief Retrieves the database password from environment variables.
         *
         * Checks if the `DB_PASSWORD` environment variable is set and returns its value.
         * Returns an empty string if not defined.
         *
         * @return The database password or an empty string if not set.
         */
        std::string getDbPasswordFromEnv();

        /**
         * @brief Gets the database host.
         *
         * @return The database host as a constant reference.
         */
        const std::string &getDbHost() const;

        /**
         * @brief Gets the database username.
         *
         * @return The database username as a constant reference.
         */
        const std::string &getDbUser() const;

        /**
         * @brief Gets the database name.
         *
         * @return The database name as a constant reference.
         */
        const std::string &getDbName() const;

        /**
         * @brief Gets the database port.
         *
         * @return The database port number.
         */
        int getDbPort() const;

        /**
         * @brief Gets the server port.
         *
         * @return The web server port number.
         */
        int getServerPort() const;

        void setServerPort(int port);

    private:
        static constexpr const char *DEFAULT_DB_HOST = "localhost";
        static constexpr const char *DEFAULT_DB_USER = "root";
        static constexpr const char *DEFAULT_DB_PASS = "";
        static constexpr const char *DEFAULT_DB_NAME = "";
        static constexpr int DEFAULT_DB_PORT = 3306;
        static constexpr int DEFAULT_SERVER_PORT = 8080;
        static const fs::path DEFAULT_CONFIG_PATH;

        std::string db_host;
        std::string db_user;
        std::string db_pass;
        std::string db_name;
        int db_port;
        int server_port;
    };
}

#endif
