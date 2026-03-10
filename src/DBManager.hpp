#ifndef DBMANAGER_HPP
#define DBMANAGER_HPP

#include <pqxx/pqxx>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <memory>

class DBManager
{
public:
    DBManager()
    {
        // Pull credentials from .env
        const char *user = std::getenv("DB_USER");
        const char *pass = std::getenv("DB_PASS");
        const char *name = std::getenv("DB_NAME");
        const char *host = std::getenv("DB_HOST");

        // Build connection string with fallbacks for safety
        std::string conn_str =
            "host=" + std::string(host ? host : "db") +
            " user=" + std::string(user ? user : "user") +
            " password=" + std::string(pass ? pass : "password") +
            " dbname=" + std::string(name ? name : "pixelcanvas");

        try
        {
            conn = std::make_unique<pqxx::connection>(conn_str);
            if (conn->is_open())
            {
                std::cout << "Connected to PostgreSQL via Environment Credentials." << std::endl;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "CRITICAL DB ERROR: " << e.what() << std::endl;
        }
    }

    // REQUIRED FOR JWT: Let main.cpp use the connection for Login/Register
    pqxx::connection &getConnection()
    {
        return *conn;
    }

    // Saves a single pixel (UPSERT)
    void savePixel(int x, int y, const std::string &color)
    {
        try
        {
            pqxx::work W(*conn);
            W.exec0("INSERT INTO canvas (x, y, color) VALUES (" +
                    W.quote(x) + ", " + W.quote(y) + ", " + W.quote(color) +
                    ") ON CONFLICT (x, y) DO UPDATE SET color = EXCLUDED.color;");
            W.commit();
        }
        catch (const std::exception &e)
        {
            std::cerr << "Pixel Save Error: " << e.what() << std::endl;
        }
    }

    // Returns the current state for initial load or reconnect sync
    std::string getFullCanvasJSON()
    {
        try
        {
            pqxx::nontransaction N(*conn);
            pqxx::result R = N.exec("SELECT x, y, color FROM canvas ORDER BY y, x;");

            nlohmann::json j = nlohmann::json::array();
            for (auto row : R)
            {
                j.push_back({{"x", row[0].as<int>()}, {"y", row[1].as<int>()}, {"color", row[2].as<std::string>()}});
            }
            return j.dump();
        }
        catch (...)
        {
            return "[]";
        }
    }

private:
    // Use unique_ptr to handle the connection lifecycle cleanly
    std::unique_ptr<pqxx::connection> conn;
};

#endif