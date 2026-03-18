#ifndef DBMANAGER_HPP
#define DBMANAGER_HPP

#include <pqxx/pqxx>
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
    DBManager(int pool_size = 10);

    /**
     * Thread-safe method to acquire a database connection from the pool.
     * Spawns a new connection on the fly if the pool is temporarily exhausted.
     * @return Unique pointer to a pqxx::connection.
     */
    std::unique_ptr<pqxx::connection> get_connection();

    /**
     * Thread-safe method to return a connection to the pool.
     * Implements "Poison Control": dead or broken connections are dropped instead of queued.
     * @param conn The connection to return.
     */
    void return_connection(std::unique_ptr<pqxx::connection> conn);

    /**
     * Persists a pixel change to the database.
     * Updated with a retry loop to prevent data loss during DB restarts.
     */
    void savePixel(int x, int y, const std::string &color);

    /**
     * Retrieves the entire current state of the 50x50 canvas.
     * @return JSON-formatted string representing the full pixel array.
     */
    std::string getFullCanvasJSON();

private:
    std::string conn_str;
    std::queue<std::unique_ptr<pqxx::connection>> pool;
    std::mutex mtx; // Mutex strictly for thread-safe access to the std::queue
};

#endif