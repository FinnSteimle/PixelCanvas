#include "RedisManager.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <chrono>

RedisManager::RedisManager() : redis(get_redis_url())
{
    std::cout << "Connected to Redis at " << get_redis_url() << std::endl;
}

void RedisManager::publishPixel(int x, int y, const std::string &color)
{
    nlohmann::json j;
    j["x"] = x;
    j["y"] = y;
    j["color"] = color;
    // Broadcast the JSON string to all subscribers
    redis.publish("canvas_updates", j.dump());
}

void RedisManager::subscribe(std::function<void(const std::string &, const std::string &)> callback)
{
    std::thread([this, callback]()
                {
        while (true) {
            try {
                auto sub = redis.subscriber();
                // Define what happens when a message arrives
                sub.on_message([callback](std::string channel, std::string msg) {
                    callback(channel, msg);
                });
                // Subscribe to the channel
                sub.subscribe("canvas_updates");
                // Keep consuming messages in a loop
                while (true) {
                    sub.consume();
                }
            } catch (const std::exception& e) {
                // Log error and attempt to reconnect after a delay
                std::cerr << "Redis Error: " << e.what() << ". Retrying..." << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(2));
            }
        } })
        .detach(); // Detach the thread to let it run independently
}

std::string RedisManager::get_redis_url()
{
    const char *host = std::getenv("REDIS_HOST");
    return "tcp://" + std::string(host ? host : "redis") + ":6379";
}