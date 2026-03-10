#include "crow.h"
#include "DBManager.hpp"
#include "RedisManager.hpp"
#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <unordered_set>
#include <mutex>

using namespace std;
using json = nlohmann::json;

mutex mtx;
unordered_set<crow::websocket::connection *> users;

string read_file(const string &path)
{
    ifstream f(path);
    if (!f.is_open())
        return "";
    stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}

int main(int argc, char *argv[])
{
    int port = 8080;
    if (argc > 1)
    {
        port = std::stoi(argv[1]);
    }

    crow::SimpleApp app;
    DBManager db;

    // Define how to broadcast to the users connected to THIS specific instance
    auto broadcast_to_local_users = [](const string &data)
    {
        lock_guard<mutex> _(mtx);
        for (auto u : users)
        {
            u->send_text(data);
        }
    };

    // Initialize Redis and give it the broadcast function
    RedisManager redis_mgr(broadcast_to_local_users);

    CROW_ROUTE(app, "/")
    ([]()
     {
        string content = read_file("/workspaces/PixelCanvas/static/index.html");
        if (content.empty()) return crow::response(404, "HTML File Not Found");
        return crow::response(content); });

    CROW_ROUTE(app, "/app.js")
    ([]()
     {
        string content = read_file("/workspaces/PixelCanvas/static/app.js");
        crow::response res(content);
        res.set_header("Content-Type", "application/javascript");
        return res; });

    CROW_ROUTE(app, "/style.css")
    ([]()
     {
        string content = read_file("/workspaces/PixelCanvas/static/style.css");
        crow::response res(content);
        res.set_header("Content-Type", "text/css");
        return res; });

    CROW_ROUTE(app, "/ws").websocket(&app).onopen([&](crow::websocket::connection &conn)
                                                  {
            lock_guard<mutex> _(mtx);
            users.insert(&conn); })
        .onclose([&](crow::websocket::connection &conn, const string &reason)
                 {
            lock_guard<mutex> _(mtx);
            users.erase(&conn); })
        .onmessage([&](crow::websocket::connection & /*conn*/, const string &data, bool is_binary)
                   {
            try {
                auto msg = json::parse(data);
                db.savePixel(msg["x"], msg["y"], msg["color"]);
                
                
                redis_mgr.publish(data); 

            } catch (const std::exception& e) {
                cout << "Error processing message: " << e.what() << endl;
            } });

    // Get canvas from database
    CROW_ROUTE(app, "/canvas")
    ([&]()
     {
        crow::response res(db.getFullCanvasJSON());
        res.set_header("Content-Type", "application/json");
        return res; });

    app.port(port).multithreaded().run();
}