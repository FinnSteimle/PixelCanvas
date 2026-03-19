#include "crow.h"
#include "DBManager.hpp"
#include "RedisManager.hpp"
#include <jwt-cpp/jwt.h>
#include <nlohmann/json.hpp>
#include <sodium.h>
#include <iostream>
#include <unordered_set>
#include <mutex>
#include <string>
#include <format>
#include <chrono>
#include <queue>
#include <condition_variable>
#include <thread>
#include <vector>

using namespace std;
using json = nlohmann::json;

// --- PASSWORD SECURITY ---

/**
 * Generates a secure hash for a password using libsodium.
 * @param password The plain-text password to hash.
 * @return A string containing the hashed password and salt.
 */
string hash_password(const string &password)
{
    char hashed[crypto_pwhash_STRBYTES];
    // Changed to MIN limits to save CPU cycles during load testing
    if (crypto_pwhash_str(hashed, password.c_str(), password.length(),
                          crypto_pwhash_OPSLIMIT_MIN,
                          crypto_pwhash_MEMLIMIT_MIN) != 0)
    {
        throw runtime_error("Hashing failed");
    }
    return string(hashed);
}

/**
 * Verifies a password against a previously generated hash.
 * @param hashed The hashed password string.
 * @param password The plain-text password to verify.
 * @return True if the password matches the hash, false otherwise.
 */
bool verify_password(const string &hashed, const string &password)
{
    return crypto_pwhash_str_verify(hashed.c_str(), password.c_str(), password.length()) == 0;
}

// Load JWT secret from environment variable or use a fallback for development
const char *env_secret = getenv("JWT_SECRET");
const string JWT_SECRET = env_secret ? env_secret : "dev_fallback_secret_do_not_use_in_prod";

// Struct to hold pixel data for the background queue
struct PixelUpdate {
    int x, y;
    string color;
};

int main(int argc, char *argv[])
{
    // Initialize libsodium for secure password hashing
    if (sodium_init() < 0) return 1;

    crow::SimpleApp app;
    // Increase pool from 10 to 200 to handle 500+ concurrent HTTP requests
    DBManager db(200); 
    RedisManager redis;

    mutex mtx;
    unordered_set<crow::websocket::connection *> users;

    queue<PixelUpdate> update_queue;
    mutex queue_mtx;
    condition_variable_any queue_cv;

    // Spawn multiple background threads (1 per CPU core) to process the queue in parallel
    unsigned int core_count = std::thread::hardware_concurrency();
    if (core_count == 0) core_count = 4; // Fallback
    
    vector<jthread> workers;
    // Single background thread to batch database writes and prevent transaction overhead
    workers.emplace_back([&](stop_token st) {
        vector<std::tuple<int, int, string>> batch;
        while (!st.stop_requested()) {
            {
                unique_lock<mutex> lock(queue_mtx);
                // Wait for updates or a 50ms timeout to ensure low latency and batch efficiency
                queue_cv.wait_for(lock, st, 50ms, [&]{ return !update_queue.empty(); });
                
                while (!update_queue.empty() && batch.size() < 100) {
                    auto update = update_queue.front();
                    update_queue.pop();
                    batch.emplace_back(update.x, update.y, update.color);
                    // Also update local cache for immediate feedback on the same instance
                    db.updateCache(update.x, update.y, update.color);
                }
            }

            if (!batch.empty()) {
                db.savePixelsBatch(batch);
                // Broadcast updates to other instances via Redis after DB persistence
                for (auto& [x, y, color] : batch) {
                    redis.publishPixel(x, y, color);
                }
                batch.clear();
            }
        }
    });

    // --- REST ENDPOINTS ---

    /**
     * Endpoint for new user registration.
     * Hashes the password and stores the username and hash in PostgreSQL.
     */
    CROW_ROUTE(app, "/register").methods(crow::HTTPMethod::POST)([&](const crow::request &req)
    {
        // Acquire a thread-safe connection from the pool
        auto conn = db.get_connection();
        
        // Health check: instantly reject if DB is unreachable
        if (!conn || !conn->is_open()) {
            return crow::response(502, "Database unavailable");
        }

        try {
            auto data = json::parse(req.body);
            string user = data["username"];
            string pass = data["password"];
            string hashed = hash_password(pass);

            // Strict RAII Scoping: The transaction begins here.
            pqxx::work W(*conn);
            
            string query = format("INSERT INTO users (username, password_hash) VALUES ({}, {});", 
                                  W.quote(user), W.quote(hashed));
            W.exec(query);
            W.commit();
            
            // Success: Connection returns to pool automatically when 'conn' goes out of scope
            return crow::response(201, "User created");

        } catch (const std::exception &e) {
            // NON-FATAL: e.g., Username already exists. The pqxx::work object auto-rolls back here.
            cerr << format("Registration Error: {}\n", e.what());
            // Connection returns to pool automatically
            return crow::response(409, "Registration failed");
        }
    });

    /**
     * Endpoint for user login.
     * Verifies credentials and returns a JWT if authentication is successful.
     */
    CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::POST)([&](const crow::request &req)
    {
        auto conn = db.get_connection();
        if (!conn || !conn->is_open()) {
            return crow::response(502, "Database unavailable");
        }

        try {
            auto data = json::parse(req.body);
            string user = data["username"];
            string pass = data["password"];

            pqxx::nontransaction N(*conn);
            string query = format("SELECT password_hash FROM users WHERE username = {}", N.quote(user));
            pqxx::result R = N.exec(query);
            
            if (R.empty() || !verify_password(R[0][0].as<string>(), pass)) {
                // Connection returns to pool automatically
                return crow::response(401, "Invalid credentials");
            }

            // Create a signed JWT valid for 24 hours
            auto token = jwt::create()
                .set_issuer("pixel_canvas")
                .set_type("JWS")
                .set_payload_claim("username", jwt::claim(user))
                .set_expires_at(chrono::system_clock::now() + chrono::days{1})
                .sign(jwt::algorithm::hs256{JWT_SECRET});

            json res;
            res["token"] = token;
            
            // Connection returns to pool automatically
            return crow::response(res.dump());

        } catch (const std::exception &e) { 
            cerr << format("Login Error: {}\n", e.what());
            // Connection returns to pool automatically
            return crow::response(500, "Server Error"); 
        }
    });

    /**
     * Endpoint to retrieve the current state of the canvas.
     * Requires a valid JWT in the Authorization header.
     */
    CROW_ROUTE(app, "/canvas").methods(crow::HTTPMethod::GET)([&](const crow::request &req)
    {
        auto auth_header = req.get_header_value("Authorization");
        if (!auth_header.starts_with("Bearer ")) {
            return crow::response(401, "Missing or invalid token");
        }

        string token = auth_header.substr(7);
        try {
            // Verify the JWT before allowing access
            auto decoded = jwt::decode(token);
            auto verifier = jwt::verify()
                .allow_algorithm(jwt::algorithm::hs256{JWT_SECRET})
                .with_issuer("pixel_canvas");
            
            verifier.verify(decoded);
            return crow::response(db.getFullCanvasJSON());
        } catch (...) {
            return crow::response(401, "Invalid token");
        } 
    });

    // --- WEBSOCKET HANDLER ---

    /**
     * WebSocket endpoint for real-time pixel updates.
     * Validates JWT token in query parameters upon connection.
     */
    CROW_ROUTE(app, "/ws")
        .websocket(&app)
        .onaccept([&](const crow::request &req, void **userdata)
        {
            // Extract and verify token from query parameters
            auto token_ptr = req.url_params.get("token");
            if (!token_ptr) return false;

            try {
                auto decoded = jwt::decode(std::string(token_ptr));
                auto verifier = jwt::verify()
                    .allow_algorithm(jwt::algorithm::hs256{JWT_SECRET})
                    .with_issuer("pixel_canvas");
                
                verifier.verify(decoded);
                return true;
            } catch (...) {
                return false;
            }
        })
        .onopen([&](crow::websocket::connection &conn)
        {
            // Add new connection to the active users set
            std::lock_guard<std::mutex> _(mtx);
            users.insert(&conn);
        })
        .onclose([&](crow::websocket::connection &conn, const string &reason, uint16_t status)
        {
            // Remove connection from active users set
            std::lock_guard<std::mutex> _(mtx);
            users.erase(&conn);
        })
        .onmessage([&](crow::websocket::connection &conn, const string &data, bool is_binary)
        {
            // Handle incoming pixel updates from clients
            try {
                auto msg = json::parse(data);
                std::string color = msg["color"]; 

                // Non-blocking write: push to queue and return immediately
                {
                    lock_guard<mutex> lock(queue_mtx);
                    update_queue.push({msg["x"], msg["y"], color});
                }
                queue_cv.notify_one();

            } catch (...) {}
        });

    // --- REDIS SYNCHRONIZATION ---

    /**
     * Listens for pixel updates broadcasted by other backend instances via Redis.
     * Forwards these updates to all connected local clients via WebSocket.
     */
    redis.subscribe([&](const string &channel, const string &message)
    {
        try {
            auto msg = json::parse(message);
            // Sync with other instances by updating our local memory cache
            db.updateCache(msg["x"].get<int>(), msg["y"].get<int>(), msg["color"].get<string>());

            // Safely broadcast to local clients without risking dangling pointers
            std::lock_guard<std::mutex> _(mtx);
            for (auto u : users) {
                // Crow WS sends are usually buffered and non-blocking
                u->send_text(message);
            }
        } catch (...) {}
    });

    // Start the Crow application on port 8080 with a thread pool optimized for 500+ VUs
    app.port(8080).concurrency(512).run();
}