#include "crow.h"
#include <sw/redis++/redis++.h>
#include <pqxx/pqxx>
#include <iostream>

int main()
{
    crow::SimpleApp app;

    CROW_ROUTE(app, "/health")([]()
                               { return "Backend is Healthy"; });

    std::cout << "PixelCanvas Server starting on port 8080..." << std::endl;
    app.port(8080).multithreaded().run();
}