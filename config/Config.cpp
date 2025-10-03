#include "Config.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace Vix
{
    namespace fs = std::filesystem;

    const std::string CONFIG_FILE_PATH = "config.json";

    Config::Config()
        : db_host("localhost"),
          db_user("root"),
          db_pass(),
          db_name(""),
          db_port(3306),
          server_port(8080)
    {
    }

    using json = nlohmann::json;

    void Config::loadConfig()
    {
        if (!fs::exists(CONFIG_FILE_PATH))
        {
            throw std::runtime_error("Le fichier de configuration n'existe pas : " + CONFIG_FILE_PATH);
        }

        std::ifstream config_file(CONFIG_FILE_PATH, std::ios::in | std::ios::binary);
        if (!config_file.is_open())
        {
            throw std::runtime_error("Impossible d'ouvrir le fichier de configuration : " + CONFIG_FILE_PATH);
        }

        json config;
        try
        {
            config_file >> config;
        }
        catch (const json::parse_error &e)
        {
            throw std::runtime_error("Erreur de parsing du fichier JSON : " + std::string(e.what()));
        }

        if (!config.contains("database") || !config.contains("server"))
        {
            throw std::runtime_error("Fichier JSON invalide : sections 'database' ou 'server' manquantes.");
        }

        try
        {
            db_host = config.at("database").at("host").get<std::string>();
            db_user = config.at("database").at("user").get<std::string>();
            db_pass = config.at("database").at("password").get<std::string>();
            db_name = config.at("database").at("name").get<std::string>();
            server_port = config.at("server").at("port").get<int>();
        }
        catch (const json::type_error &e)
        {
            throw std::runtime_error("Types incorrects dans le fichier JSON : " + std::string(e.what()));
        }
    }

    std::shared_ptr<sql::Connection> Config::getDbConnection()
    {
        try
        {
            sql::mysql::MySQL_Driver *driver = sql::mysql::get_mysql_driver_instance();
            if (!driver)
            {
                throw std::runtime_error("Impossible de récupérer le driver MySQL.");
            }

            std::shared_ptr<sql::Connection> con(
                driver->connect("tcp://" + db_host + ":" + std::to_string(db_port), db_user, db_pass));
            if (!con)
            {
                throw std::runtime_error("La connexion à la base de données a échoué.");
            }

            con->setSchema(getDbName());
            return con;
        }
        catch (const sql::SQLException &e)
        {
            throw std::runtime_error("Erreur de connexion à la base de données.");
        }
    }

    std::string Config::getDbPasswordFromEnv()
    {
        const char *password = std::getenv("DB_PASSWORD");
        if (password == nullptr)
        {
            std::cerr << "Aucun mot de passe DB_PASSWORD trouvé dans les variables d'environnement, utilisation d'un mot de passe par défaut." << std::endl;
        }
        return std::string(password);
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

    const std::string &Config::getDbHost() const { return db_host; }
    const std::string &Config::getDbUser() const { return db_user; }
    const std::string &Config::getDbName() const { return db_name; }
    int Config::getDbPort() const { return db_port; }
    int Config::getServerPort() const { return server_port; }

    Config &Config::getInstance()
    {
        static Config instance;
        return instance;
    }

}