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

    // Pre-seed the memory cache to avoid any database lookups during the load test
    auto conn = get_connection();
    if (conn && conn->is_open()) {
        try {
            pqxx::nontransaction N(*conn);
            pqxx::result R = N.exec("SELECT x, y, color FROM canvas;");
            for (auto row : R) {
                int x = row[0].as<int>();
                int y = row[1].as<int>();
                if (x >= 0 && x < 50 && y >= 0 && y < 50) {
                    canvas_cache[x][y] = row[2].as<std::string>();
                }
            }
            std::cout << "Canvas cache pre-seeded successfully.\n";
        } catch (...) {
            std::cerr << "Failed to pre-seed canvas cache.\n";
        }
    }
}

DBManager::ConnectionPtr DBManager::get_connection()
{
    std::unique_lock<std::mutex> lock(mtx);
    // Added a 2-second timeout to prevent deadlocking the entire server if the DB hangs
    if (!cv.wait_for(lock, 2s, [this] { return !pool.empty(); })) {
        std::cerr << "Database connection pool timeout! Request will likely fail.\n";
        return ConnectionPtr(nullptr, [](pqxx::connection*){});
    }
    
    // Extract the raw connection to wrap it in our auto-returning smart pointer
    auto raw_conn = std::move(pool.front());
    pool.pop();

    return ConnectionPtr(raw_conn.release(), [this](pqxx::connection* conn) {
        this->return_connection(conn);
    });
}

void DBManager::return_connection(pqxx::connection* conn)
{
    if (!conn) return;

    if (conn->is_open()) {
        {
            std::lock_guard<std::mutex> lock(mtx);
            pool.push(std::unique_ptr<pqxx::connection>(conn));
        }
        cv.notify_one();
    } else {
        // Poison Control: If a connection died, delete it and try to replace it
        delete conn;
        try {
            auto new_conn = std::make_unique<pqxx::connection>(conn_str);
            if (new_conn->is_open()) {
                std::lock_guard<std::mutex> lock(mtx);
                pool.push(std::move(new_conn));
            }
        } catch (...) {}
        cv.notify_one();
    }
}

void DBManager::savePixel(int x, int y, std::string_view color)
{
    // Update the memory cache immediately for instant visibility
    updateCache(x, y, color);

    // Persist to DB using a thread-safe connection from the pool
    auto conn = get_connection();
    if (!conn || !conn->is_open()) return;

    try {
        pqxx::work W(*conn);
        std::string query = std::format(
            "INSERT INTO canvas (x, y, color) VALUES ({}, {}, {}) "
            "ON CONFLICT (x, y) DO UPDATE SET color = EXCLUDED.color;",
            W.quote(x), W.quote(y), W.quote(color)
        );
        W.exec(query);
        W.commit();
    } catch (const std::exception &e) {
        std::cerr << std::format("Failed to save pixel: {}\n", e.what());
    }
}

std::string DBManager::getFullCanvasJSON()
{
    std::lock_guard<std::mutex> lock(cache_mtx);
    
    // Only rebuild the JSON string if the cache has been modified since the last request
    if (cache_dirty || cached_json.empty()) {
        nlohmann::json j = nlohmann::json::array();
        for (int y = 0; y < 50; ++y) {
            for (int x = 0; x < 50; ++x) {
                nlohmann::json pixel;
                pixel["x"] = x;
                pixel["y"] = y;
                pixel["color"] = canvas_cache[x][y].empty() ? "#FFFFFF" : canvas_cache[x][y];
                j.push_back(pixel);
            }
        }
        cached_json = j.dump();
        cache_dirty = false;
    }
    return cached_json;
}

void DBManager::updateCache(int x, int y, std::string_view color)
{
    if (x < 0 || x >= 50 || y < 0 || y >= 50) return;
    std::lock_guard<std::mutex> lock(cache_mtx);
    if (canvas_cache[x][y] != color) {
        canvas_cache[x][y] = color;
        cache_dirty = true;
    }
}

void DBManager::savePixelsBatch(const std::vector<std::tuple<int, int, std::string>>& updates)
{
    if (updates.empty()) return;

    auto conn = get_connection();
    if (!conn || !conn->is_open()) return;

    try {
        pqxx::work W(*conn);
        std::string query = "INSERT INTO canvas (x, y, color) VALUES ";
        for (size_t i = 0; i < updates.size(); ++i) {
            auto [x, y, color] = updates[i];
            query += std::format("({}, {}, {})", W.quote(x), W.quote(y), W.quote(color));
            if (i < updates.size() - 1) query += ", ";
        }
        query += " ON CONFLICT (x, y) DO UPDATE SET color = EXCLUDED.color;";
        W.exec(query);
        W.commit();
    } catch (const std::exception &e) {
        std::cerr << std::format("Failed to save pixel batch: {}\n", e.what());
    }
}