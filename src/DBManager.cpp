#include "DBManager.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <chrono>

DBManager::DBManager(int pool_size)
{
    const char *user = std::getenv("DB_USER");
    const char *pass = std::getenv("DB_PASS");
    const char *name = std::getenv("DB_NAME");
    const char *host = std::getenv("DB_HOST");

    // Format connection string for libpqxx
    conn_str = "host=" + std::string(host ? host : "db") +
               " user=" + std::string(user ? user : "user") +
               " password=" + std::string(pass ? pass : "password") +
               " dbname=" + std::string(name ? name : "pixelcanvas");

    for (int i = 0; i < pool_size; ++i) {
        try {
            auto conn = std::make_unique<pqxx::connection>(conn_str);
            if (conn->is_open()) {
                pool.push(std::move(conn));
            }
        } catch (const std::exception &e) {
            std::cerr << "DB Pool Init Error: " << e.what() << std::endl;
        }
    }
    std::cout << "DB Pool initialized with " << pool.size() << " connections." << std::endl;
}

std::unique_ptr<pqxx::connection> DBManager::get_connection()
{
    std::lock_guard<std::mutex> lock(mtx);
    if (pool.empty()) {
        try {
            return std::make_unique<pqxx::connection>(conn_str);
        } catch (...) {
            return nullptr;
        }
    }
    auto conn = std::move(pool.front());
    pool.pop();
    return conn;
}

void DBManager::return_connection(std::unique_ptr<pqxx::connection> conn)
{
    if (conn && conn->is_open()) {
        std::lock_guard<std::mutex> lock(mtx);
        pool.push(std::move(conn));
    }
    // If conn is broken/null, it automatically goes out of scope here and is destroyed safely.
}

void DBManager::savePixel(int x, int y, const std::string &color)
{
    const int max_retries = 3;
    
    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        auto conn = get_connection();
        
        // If DB is down, wait 100ms and try again
        if (!conn || !conn->is_open()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        try {
            pqxx::work W(*conn);
            W.exec("INSERT INTO canvas (x, y, color) VALUES (" +
                    W.quote(x) + ", " + W.quote(y) + ", " + W.quote(color) +
                    ") ON CONFLICT (x, y) DO UPDATE SET color = EXCLUDED.color;");
            W.commit();
            
            return_connection(std::move(conn));
            return; // Pixel saved successfully
            
        } catch (const pqxx::broken_connection& e) {
            // Connection died mid-query. Drop the bad connection and retry.
            std::cerr << "DB connection lost, retrying pixel save (" << attempt << "/" << max_retries << ")\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        } catch (const std::exception& e) {
            // Non-connection error (SQL syntax, etc.) - return connection and stop
            std::cerr << "SQL Error in savePixel: " << e.what() << std::endl;
            return_connection(std::move(conn));
            return;
        }
    }
    std::cerr << "Critical: Pixel update lost after " << max_retries << " failed attempts.\n";
}

std::string DBManager::getFullCanvasJSON()
{
    auto conn = get_connection();
    if (!conn || !conn->is_open()) return "[]";

    try {
        pqxx::nontransaction N(*conn);
        // Select all pixels, ordered systematically
        pqxx::result R = N.exec("SELECT x, y, color FROM canvas ORDER BY y, x;");

        nlohmann::json j = nlohmann::json::array();
        for (auto row : R) {
            // Push each database row into the JSON array
            j.push_back({{"x", row[0].as<int>()}, {"y", row[1].as<int>()}, {"color", row[2].as<std::string>()}});
        }
        
        return_connection(std::move(conn));
        return j.dump(); // Convert JSON object to string for transmission
        
    } catch (...) {
        return "[]"; // Return empty array on failure
    }
}