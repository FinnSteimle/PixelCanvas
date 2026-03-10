#include "crow.h"
#include "DBManager.hpp"
#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <unordered_set>
#include <mutex>

using namespace std;
using json = nlohmann::json;

mutex mtx;
unordered_set<crow::websocket::connection *> users;

// Helper function to read a file manually
string read_file(const string &path)
{
    ifstream f(path);
    if (!f.is_open())
        return "";
    stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}

int main()
{
    crow::SimpleApp app;
    DBManager db;

    // Serve index.html
    CROW_ROUTE(app, "/")
    ([]()
     {
        string content = read_file("/workspaces/PixelCanvas/static/index.html");
        if (content.empty()) return crow::response(404, "HTML File Not Found");
        return crow::response(content); });

    // Serve app.js
    CROW_ROUTE(app, "/app.js")
    ([]()
     {
        string content = read_file("/workspaces/PixelCanvas/static/app.js");
        crow::response res(content);
        res.set_header("Content-Type", "application/javascript");
        return res; });

    // Serve style.css
    CROW_ROUTE(app, "/style.css")
    ([]()
     {
        string content = read_file("/workspaces/PixelCanvas/static/style.css");
        crow::response res(content);
        res.set_header("Content-Type", "text/css");
        return res; });

    // WebSocket
    CROW_ROUTE(app, "/ws").websocket(&app).onopen([&](crow::websocket::connection &conn)
                                                  {
            lock_guard<mutex> _(mtx);
            users.insert(&conn); })
        .onclose([&](crow::websocket::connection &conn, const string &reason)
                 {
            lock_guard<mutex> _(mtx);
            users.erase(&conn); })
        .onmessage([&](crow::websocket::connection & /*conn*/, const string &data, bool is_binary) { // FIXED: Just [&]
            try
            {
                auto msg = json::parse(data);
                int x = msg["x"];
                int y = msg["y"];
                string color = msg["color"];

                db.savePixel(x, y, color); // Saves to Postgres

                lock_guard<mutex> _(mtx);
                for (auto u : users)
                    u->send_text(data); // Broadcasts to everyone
            }
            catch (const std::exception &e)
            {
                cout << "Error processing message: " << e.what() << endl;
            }
        });

    // Get canvas from database
    CROW_ROUTE(app, "/canvas")
    ([&]()
     { 
        crow::response res(db.getFullCanvasJSON());
        res.set_header("Content-Type", "application/json");
        return res; });

    app.port(8080).multithreaded().run();
}