#ifndef DBMANAGER_HPP
#define DBMANAGER_HPP

#include <pqxx/pqxx>
#include <string>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <string_view>
#include <functional>

/**
 * Manages a thread-safe connection pool to the PostgreSQL database.
 * Handles user authentication storage, canvas state, and poison control.
 */
class DBManager
{
public:
    // Smart pointer type that handles its own cleanup back into the pool
    using ConnectionPtr = std::unique_ptr<pqxx::connection, std::function<void(pqxx::connection*)>>;

    /**
     * Initializes the connection pool with a predefined number of sockets.
     * @param pool_size Number of simultaneous database connections to maintain.
     */
    DBManager(int pool_size = 10);

    /**
     * Thread-safe method to acquire a database connection from the pool.
     * Blocks the calling thread if the pool is empty until a connection returns.
     * @return A ConnectionPtr (smart pointer) that returns to the pool automatically.
     */
    ConnectionPtr get_connection();

    /**
     * Persists a pixel change to the database.
     * Updated with a retry loop to prevent data loss during DB restarts.
     */
    void savePixel(int x, int y, std::string_view color);

    /**
     * Retrieves the entire current state of the 50x50 canvas.
     * Uses an in-memory cache to handle high-concurrency requests.
     * @return JSON-formatted string representing the full pixel array.
     */
    std::string getFullCanvasJSON();

    /**
     * Updates the in-memory cache and invalidates the JSON string cache.
     */
    void updateCache(int x, int y, std::string_view color);

    /**
     * Persists multiple pixel changes to the database in a single transaction.
     */
    void savePixelsBatch(const std::vector<std::tuple<int, int, std::string>>& updates);

private:
    // Internal handler to put raw pointers back into the unique_ptr pool
    void return_connection(pqxx::connection* conn);
    
    // In-memory cache of the canvas state to avoid DB thrashing
    std::string canvas_cache[50][50];
    std::string cached_json;
    bool cache_dirty = true;
    std::mutex cache_mtx;

    std::string conn_str;
    std::queue<std::unique_ptr<pqxx::connection>> pool;
    std::mutex mtx;
    std::condition_variable cv;
};

#endif