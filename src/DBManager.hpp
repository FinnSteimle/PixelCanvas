#ifndef DBMANAGER_HPP
#define DBMANAGER_HPP

#include <pqxx/pqxx>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <vector>

class DBManager
{
public:
    // Connection string points to the 'db' service defined in docker-compose
    DBManager() : conn("host=127.0.0.1 port=5432 dbname=pixelcanvas user=user password=password")
    {
        if (conn.is_open())
        {
            std::cout << "Connected to PostgreSQL: " << conn.dbname() << std::endl;
        }
    }

    // Saves a single pixel to the database
    void savePixel(int x, int y, const std::string &color)
    {
        try
        {
            pqxx::work W(conn);
            // UPSERT: Insert the pixel, or update the color if (x, y) already exists
            std::string sql = "INSERT INTO canvas (x, y, color) VALUES (" +
                              W.quote(x) + ", " + W.quote(y) + ", " + W.quote(color) +
                              ") ON CONFLICT (x, y) DO UPDATE SET color = EXCLUDED.color;";
            W.exec(sql);
            W.commit();
        }
        catch (const std::exception &e)
        {
            std::cerr << "DB Error: " << e.what() << std::endl;
        }
    }

    std::string getFullCanvasJSON()
    {
        try
        {
            pqxx::nontransaction N(conn); // 'nontransaction' is faster for read-only
            pqxx::result R = N.exec("SELECT x, y, color FROM canvas ORDER BY y, x;");

            nlohmann::json j = nlohmann::json::array();
            for (auto row : R)
            {
                j.push_back({{"x", row[0].as<int>()},
                             {"y", row[1].as<int>()},
                             {"color", row[2].as<std::string>()}});
            }
            return j.dump(); // Converts the JSON object to a string
        }
        catch (const std::exception &e)
        {
            std::cerr << "Fetch Error: " << e.what() << std::endl;
            return "[]";
        }
    }

private:
    pqxx::connection conn;
};

#endif