#include "crow.h"
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <mutex>

using namespace std;

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
        .onmessage([&](crow::websocket::connection & /*conn*/, const string &data, bool is_binary)
                   {
            lock_guard<mutex> _(mtx);
            for (auto u : users) {
                u->send_text(data);
            } });

    app.port(8080).multithreaded().run();
}