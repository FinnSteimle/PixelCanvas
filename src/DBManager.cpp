#include "DBManager.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <format>

using namespace std::chrono_literals;

DBManager::DBManager(int pool_size)
{
    const char *user = std::getenv("DB_USER");
    const char *pass = std::getenv("DB_PASS");
    const char *name = std::getenv("DB_NAME");
    const char *host = std::getenv("DB_HOST");

    // Format connection string for libpqxx using C++20 std::format
    conn_str = std::format("host={} user={} password={} dbname={}",
                           host ? host : "db",
                           user ? user : "user",
                           pass ? pass : "password",
                           name ? name : "pixelcanvas");

    for (int i = 0; i < pool_size; ++i) {
        try {
            auto conn = std::make_unique<pqxx::connection>(conn_str);
            if (conn->is_open()) {
                pool.push(std::move(conn));
            }
        } catch (const std::exception &e) {
            std::cerr << std::format("DB Pool Init Error: {}\n", e.what());
        }
    }
    std::cout << std::format("DB Pool initialized with {} connections.\n", pool.size());
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

void DBManager::savePixel(int x, int y, std::string_view color)
{
    constexpr int max_retries = 3;
    
    for (int attempt = 1; attempt <= max_retries; ++attempt) {
        auto conn = get_connection();
        
        // If DB is down, wait 100ms and try again
        if (!conn || !conn->is_open()) {
            std::this_thread::sleep_for(100ms);
            continue;
        }

        try {
            pqxx::work W(*conn);
            
            std::string query = std::format(
                "INSERT INTO canvas (x, y, color) VALUES ({}, {}, {}) "
                "ON CONFLICT (x, y) DO UPDATE SET color = EXCLUDED.color;",
                W.quote(x), W.quote(y), W.quote(color)
            );
            
            W.exec(query);
            W.commit();
            
            return_connection(std::move(conn));
            return; // Pixel saved successfully
            
        } catch (const pqxx::broken_connection& e) {
            // Connection died mid-query. Drop the bad connection and retry.
            std::cerr << std::format("DB connection lost, retrying pixel save ({}/{})\n", attempt, max_retries);
            std::this_thread::sleep_for(100ms);
        } catch (const std::exception& e) {
            // Non-connection error (SQL syntax, etc.) - return connection and stop
            std::cerr << std::format("SQL Error in savePixel: {}\n", e.what());
            return_connection(std::move(conn));
            return;
        }
    }
    std::cerr << std::format("Critical: Pixel update lost after {} failed attempts.\n", max_retries);
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
        for (const auto& row : R) {
            // Push each database row into the JSON array
            j.push_back({
                {"x", row[0].as<int>()}, 
                {"y", row[1].as<int>()}, 
                {"color", row[2].as<std::string>()}
            });
        }
        
        return_connection(std::move(conn));
        return j.dump(); // Convert JSON object to string for transmission
        
    } catch (...) {
        return "[]"; // Return empty array on failure
    }
}