#ifndef REDISMANAGER_HPP
#define REDISMANAGER_HPP

#include <sw/redis++/redis++.h>
#include <iostream>
#include <string>
#include <thread>
#include <functional>
#include <chrono>

class RedisManager
{
public:
    // Initialize Redis using environment variables for the host
    RedisManager() : redis(get_redis_url())
    {
        std::cout << "Connected to Redis at " << get_redis_url() << std::endl;
    }

    // This matches the call in your updated main.cpp
    void publishPixel(int x, int y, const std::string &color)
    {
        nlohmann::json j;
        j["x"] = x;
        j["y"] = y;
        j["color"] = color;
        redis.publish("canvas_updates", j.dump());
    }

    // The subscription loop now runs in a dedicated method called by main.cpp
    void subscribe(std::function<void(const std::string &, const std::string &)> callback)
    {
        std::thread([this, callback]()
                    {
            while (true) {
                try {
                    auto sub = redis.subscriber();
                    
                    sub.on_message([callback](std::string channel, std::string msg) {
                        callback(channel, msg);
                    });

                    sub.subscribe("canvas_updates");
                    std::cout << "Redis Subscriber active on 'canvas_updates'" << std::endl;

                    while (true) {
                        sub.consume(); // Blocks until message arrives
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Redis Sub Error: " << e.what() << ". Retrying in 2s..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            } })
            .detach();
    }

private:
    sw::redis::Redis redis;

    // Helper to pull Redis host from environment (useful if you change the service name)
    std::string get_redis_url()
    {
        const char *host = std::getenv("REDIS_HOST");
        return "tcp://" + std::string(host ? host : "redis") + ":6379";
    }
};

#endif