// --- src/DBManager.hpp ---

#ifndef DBMANAGER_HPP
#define DBMANAGER_HPP

#include <pqxx/pqxx>
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>
#include <memory>
#include <queue>
#include <mutex>

/**
 * Manages a thread-safe connection pool to the PostgreSQL database.
 * Handles user authentication storage, canvas state, and poison control.
 */
class DBManager
{
public:
    /**
     * Initializes the connection pool with a predefined number of sockets.
     * @param pool_size Number of simultaneous database connections to maintain.
     */
    DBManager(int pool_size = 10)
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

    /**
     * Thread-safe method to acquire a database connection from the pool.
     * Spawns a new connection on the fly if the pool is temporarily exhausted.
     * @return Unique pointer to a pqxx::connection.
     */
    std::unique_ptr<pqxx::connection> get_connection()
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

    /**
     * Thread-safe method to return a connection to the pool.
     * Implements "Poison Control": dead or broken connections are dropped instead of queued.
     * @param conn The connection to return.
     */
    void return_connection(std::unique_ptr<pqxx::connection> conn)
    {
        if (conn && conn->is_open()) {
            std::lock_guard<std::mutex> lock(mtx);
            pool.push(std::move(conn));
        }
        // If conn is broken/null, it automatically goes out of scope here and is destroyed safely.
    }

    /**
     * Persists a pixel change to the database using an ephemeral connection.
     * Uses UPSERT logic (ON CONFLICT) to update color if coordinates exist.
     * @param x X-coordinate of the pixel.
     * @param y Y-coordinate of the pixel.
     * @param color Hex color string of the pixel.
     */
    void savePixel(int x, int y, const std::string &color)
    {
        auto conn = get_connection();
        if (!conn || !conn->is_open()) return;

        try {
            pqxx::work W(*conn);
            W.exec("INSERT INTO canvas (x, y, color) VALUES (" +
                    W.quote(x) + ", " + W.quote(y) + ", " + W.quote(color) +
                    ") ON CONFLICT (x, y) DO UPDATE SET color = EXCLUDED.color;");
            W.commit();
            return_connection(std::move(conn));
        } catch (const pqxx::broken_connection& e) {
            // Connection severed. Let it drop to avoid poisoning the pool.
            std::cerr << "savePixel DB drop: " << e.what() << std::endl;
        } catch (...) {
            // Non-fatal error. Return healthy connection to pool.
            return_connection(std::move(conn));
        }
    }

    /**
     * Retrieves the entire current state of the 50x50 canvas.
     * @return JSON-formatted string representing the full pixel array.
     */
    std::string getFullCanvasJSON()
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
            
        } catch (const pqxx::broken_connection& e) {
            std::cerr << "getFullCanvasJSON DB drop: " << e.what() << std::endl;
            return "[]";
        } catch (...) {
            return_connection(std::move(conn));
            return "[]";
        }
    }

private:
    std::string conn_str;
    std::queue<std::unique_ptr<pqxx::connection>> pool;
    std::mutex mtx; // Mutex strictly for thread-safe access to the std::queue
};

#endif