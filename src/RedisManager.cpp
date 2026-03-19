#include "RedisManager.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <format>

using namespace std::chrono_literals;

RedisManager::RedisManager() : redis(get_redis_url())
{
    std::cout << std::format("Connected to Redis at {}\n", get_redis_url());
}

void RedisManager::publishPixel(int x, int y, std::string_view color)
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
    // Use jthread for RAII-based thread management and cooperative cancellation
    listener_thread = std::jthread([this, callback](std::stop_token stop_token)
    {
        while (!stop_token.stop_requested()) {
            try {
                auto sub = redis.subscriber();
                // Define what happens when a message arrives
                sub.on_message([callback](std::string channel, std::string msg) {
                    callback(channel, msg);
                });
                
                // Subscribe to the channel
                sub.subscribe("canvas_updates");
                
                // Keep consuming messages until a connection error or stop is requested
                while (!stop_token.stop_requested()) {
                    sub.consume();
                }
            } catch (const std::exception& e) {
                // Log error and attempt to reconnect after a delay using C++20 chrono literals
                std::cerr << std::format("Redis Error: {}. Retrying...\n", e.what());
                std::this_thread::sleep_for(2s);
            }
        } 
    });
}

std::string RedisManager::get_redis_url()
{
    const char *host = std::getenv("REDIS_HOST");
    // Use std::format for cleaner string construction
    return std::format("tcp://{}:6379", host ? host : "redis");
}