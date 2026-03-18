// --- src/main.cpp ---

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
    if (crypto_pwhash_str(hashed, password.c_str(), password.length(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0)
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

int main(int argc, char *argv[])
{
    // Initialize libsodium for secure password hashing
    if (sodium_init() < 0) return 1;

    crow::SimpleApp app;
    DBManager db;
    RedisManager redis;

    // Track active WebSocket connections for broadcasting updates
    mutex mtx;
    unordered_set<crow::websocket::connection *> users;

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
            W.exec("INSERT INTO users (username, password_hash) VALUES (" + 
                   W.quote(user) + ", " + W.quote(hashed) + ");");
            W.commit();
            
            // Success: Safely return the connection to the pool
            db.return_connection(std::move(conn));
            return crow::response(201, "User created");

        } catch (const pqxx::broken_connection &e) {
            // FATAL: Connection died mid-transaction. Let the unique_ptr destroy it (Poison Control).
            std::cerr << "Registration Broken Conn: " << e.what() << std::endl;
            return crow::response(502, "Database connection lost");

        } catch (const std::exception &e) {
            // NON-FATAL: e.g., Username already exists. The pqxx::work object auto-rolls back here.
            std::cerr << "Registration Error: " << e.what() << std::endl;
            db.return_connection(std::move(conn));
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
            pqxx::result R = N.exec("SELECT password_hash FROM users WHERE username = " + N.quote(user));
            
            if (R.empty() || !verify_password(R[0][0].as<string>(), pass)) {
                db.return_connection(std::move(conn));
                return crow::response(401, "Invalid credentials");
            }

            // Create a signed JWT valid for 24 hours
            auto token = jwt::create()
                .set_issuer("pixel_canvas")
                .set_type("JWS")
                .set_payload_claim("username", jwt::claim(user))
                .set_expires_at(chrono::system_clock::now() + chrono::hours{24})
                .sign(jwt::algorithm::hs256{JWT_SECRET});

            json res;
            res["token"] = token;
            
            db.return_connection(std::move(conn));
            return crow::response(res.dump());

        } catch (const pqxx::broken_connection &e) { 
            std::cerr << "Login Broken Conn: " << e.what() << std::endl;
            return crow::response(502, "Database connection lost"); 

        } catch (const std::exception &e) { 
            std::cerr << "Login Error: " << e.what() << std::endl;
            db.return_connection(std::move(conn));
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
        if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
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
                // Save the pixel change to the database
                db.savePixel(msg["x"], msg["y"], msg["color"]);
                // Broadcast the change via Redis for other backend instances
                redis.publishPixel(msg["x"], msg["y"], msg["color"]);
            } catch (...) {}
        });

    // --- REDIS SYNCHRONIZATION ---

    /**
     * Listens for pixel updates broadcasted by other backend instances via Redis.
     * Forwards these updates to all connected local clients via WebSocket.
     */
    redis.subscribe([&](const string &channel, const string &message)
    {
        lock_guard<mutex> _(mtx);
        for (auto u : users) {
            u->send_text(message);
        }
    });

    // Start the Crow application on port 8080 in multi-threaded mode
    app.port(8080).multithreaded().run();
}