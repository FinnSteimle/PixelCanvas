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

bool verify_password(const string &hashed, const string &password)
{
    return crypto_pwhash_str_verify(hashed.c_str(), password.c_str(), password.length()) == 0;
}

const char *env_secret = getenv("JWT_SECRET");
const string JWT_SECRET = env_secret ? env_secret : "dev_fallback_secret_do_not_use_in_prod";

int main(int argc, char *argv[])
{
    if (sodium_init() < 0) return 1;

    crow::SimpleApp app;
    DBManager db;
    RedisManager redis;

    mutex mtx;
    unordered_set<crow::websocket::connection *> users;

    CROW_ROUTE(app, "/register").methods(crow::HTTPMethod::POST)([&](const crow::request &req)
    {
        try {
            auto data = json::parse(req.body);
            string user = data["username"];
            string pass = data["password"];
            string hashed = hash_password(pass);

            pqxx::work W(db.getConnection());
            W.exec("INSERT INTO users (username, password_hash) VALUES (" + 
                   W.quote(user) + ", " + W.quote(hashed) + ");");
            W.commit();
            return crow::response(201, "User created");
        } catch (...) {
            return crow::response(409, "Registration failed");
        }
    });

    CROW_ROUTE(app, "/login").methods(crow::HTTPMethod::POST)([&](const crow::request &req)
    {
        try {
            auto data = json::parse(req.body);
            string user = data["username"];
            string pass = data["password"];

            pqxx::nontransaction N(db.getConnection());
            pqxx::result R = N.exec("SELECT password_hash FROM users WHERE username = " + N.quote(user));
            
            if (R.empty() || !verify_password(R[0][0].as<string>(), pass)) {
                return crow::response(401, "Invalid credentials");
            }

            auto token = jwt::create()
                .set_issuer("pixel_canvas")
                .set_type("JWS")
                .set_payload_claim("username", jwt::claim(user))
                .set_expires_at(chrono::system_clock::now() + chrono::hours{24})
                .sign(jwt::algorithm::hs256{JWT_SECRET});

            json res;
            res["token"] = token;
            return crow::response(res.dump());
        } catch (...) { 
            return crow::response(500, "Server Error"); 
        }
    });

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
        } 
    });

    CROW_ROUTE(app, "/ws")
        .websocket(&app)
        .onaccept([&](const crow::request &req, void **userdata)
        {
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
            std::lock_guard<std::mutex> _(mtx);
            users.insert(&conn);
        })
        .onclose([&](crow::websocket::connection &conn, const string &reason, uint16_t status)
        {
            std::lock_guard<std::mutex> _(mtx);
            users.erase(&conn);
        })
        .onmessage([&](crow::websocket::connection &conn, const string &data, bool is_binary)
        {
            try {
                auto msg = json::parse(data);
                db.savePixel(msg["x"], msg["y"], msg["color"]);
                redis.publishPixel(msg["x"], msg["y"], msg["color"]);
            } catch (...) {}
        });

    redis.subscribe([&](const string &channel, const string &message)
    {
        lock_guard<mutex> _(mtx);
        for (auto u : users) {
            u->send_text(message);
        }
    });

    app.port(8080).multithreaded().run();
}
