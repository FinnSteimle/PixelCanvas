#include "crow.h"
#include "DBManager.hpp"
#include "RedisManager.hpp"
#include <jwt-cpp/jwt.h>
#include <nlohmann/json.hpp>
#include <openssl/sha.h>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <unordered_set>
#include <mutex>

using namespace std;
using json = nlohmann::json;

// Professional SHA-256 hashing helper
string hash_password(const string &password)
{
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, password.c_str(), password.size());
    SHA256_Final(hash, &sha256);

    stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
        ss << hex << setw(2) << setfill('0') << (int)hash[i];
    }
    return ss.str();
}

// Global Secret (Pulled from .env via Docker)
const char *env_secret = getenv("JWT_SECRET");
const string JWT_SECRET = env_secret ? env_secret : "dev_fallback_secret_do_not_use_in_prod";

int main(int argc, char *argv[])
{
    crow::SimpleApp app;
    DBManager db;
    RedisManager redis;

    mutex mtx;
    unordered_set<crow::websocket::connection *> users;

    // REGISTER: Hash password and store
    CROW_ROUTE(app, "/register").methods(crow::HTTPMethod::POST)([&](const crow::request &req)
                                                                 {
        try {
            auto data = json::parse(req.body);
            string user = data["username"];
            string pass = data["password"];
            string hashed = hash_password(pass);

            pqxx::work W(db.getConnection());
            W.exec0("INSERT INTO users (username, password_hash) VALUES (" + 
                   W.quote(user) + ", " + W.quote(hashed) + ");");
            W.commit();
            return crow::response(201, "User created");
        } catch (...) {
            return crow::response(409, "Registration failed");
        } });

    // LOGIN: Verify hash and return JWT
    CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::POST)([&](const crow::request &req)
                                                              {
        try {
            auto data = json::parse(req.body);
            string user = data["username"];
            string pass = data["password"];
            string hashed = hash_password(pass);

            pqxx::nontransaction N(db.getConnection());
            pqxx::result R = N.exec("SELECT id FROM users WHERE username = " + 
                                    N.quote(user) + " AND password_hash = " + N.quote(hashed));
            
            if (R.empty()) return crow::response(401, "Invalid credentials");

            auto token = jwt::create()
                .set_issuer("pixel_canvas")
                .set_type("JWS")
                .set_payload_claim("username", jwt::claim(user))
                .set_expires_at(chrono::system_clock::now() + chrono::hours{24})
                .sign(jwt::algorithm::hs256{JWT_SECRET});

            json res;
            res["token"] = token;
            return crow::response(res.dump());
        } catch (...) { return crow::response(500, "Server Error"); } });

    CROW_ROUTE(app, "/canvas").methods(crow::HTTPMethod::GET)([&](const crow::request &req)
                                                              {
    auto auth_header = req.get_header_value("Authorization");
    if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
        return crow::response(401, "Missing or invalid token");
    }

    string token = auth_header.substr(7);
    try {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
            .allow_algorithm(jwt::algorithm::hs256{JWT_SECRET})
            .with_issuer("pixel_canvas");
        
        verifier.verify(decoded);
        return crow::response(db.getFullCanvasJSON());
    } catch (...) {
        return crow::response(401, "Invalid token");
    } });

    // WEBSOCKET (PROTECTED)
    CROW_ROUTE(app, "/ws")
        .websocket(&app)
        .onaccept([&](const crow::request &req, void **userdata)
                  {
            auto token_ptr = req.url_params.get("token");
            if (!token_ptr) return false; // Reject connection

            try {
                auto decoded = jwt::decode(std::string(token_ptr));
                auto verifier = jwt::verify()
                    .allow_algorithm(jwt::algorithm::hs256{JWT_SECRET})
                    .with_issuer("pixel_canvas");
                
                verifier.verify(decoded);
                return true; // Authorize and upgrade to WebSocket
            } catch (...) {
                return false; // Reject connection
            } })
        .onopen([&](crow::websocket::connection &conn)
                {
            std::lock_guard<std::mutex> _(mtx);
            users.insert(&conn);
            std::cout << "Authorized user connected!" << std::endl; })
        .onclose([&](crow::websocket::connection &conn, const string &reason)
                 {
            std::lock_guard<std::mutex> _(mtx);
            users.erase(&conn); })
        .onmessage([&](crow::websocket::connection &conn, const string &data, bool is_binary)
                   {
            try {
                auto msg = json::parse(data);
                db.savePixel(msg["x"], msg["y"], msg["color"]);
                redis.publishPixel(msg["x"], msg["y"], msg["color"]);
            } catch (...) {} });

    // Start Redis subscription loop
    redis.subscribe([&](const string &channel, const string &message)
                    {
        lock_guard<mutex> _(mtx);
        for (auto u : users) {
            u->send_text(message);
        } });

    app.port(8080).multithreaded().run();
}