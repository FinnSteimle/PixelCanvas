#ifndef REDISMANAGER_HPP
#define REDISMANAGER_HPP

#include <sw/redis++/redis++.h>
#include <iostream>
#include <string>
#include <thread>
#include <functional>
#include <chrono>

/**
 * Manages connections and operations with the Redis message broker.
 * Used for real-time synchronization between multiple backend instances.
 */
class RedisManager
{
public:
    /**
     * Initializes the Redis connection using the provided URL.
     */
    RedisManager() : redis(get_redis_url())
    {
        std::cout << "Connected to Redis at " << get_redis_url() << std::endl;
    }

    /**
     * Publishes a pixel update to the 'canvas_updates' channel.
     * @param x X-coordinate of the pixel.
     * @param y Y-coordinate of the pixel.
     * @param color Hex color string of the pixel.
     */
    void publishPixel(int x, int y, const std::string &color)
    {
        nlohmann::json j;
        j["x"] = x;
        j["y"] = y;
        j["color"] = color;
        // Broadcast the JSON string to all subscribers
        redis.publish("canvas_updates", j.dump());
    }

    /**
     * Subscribes to the 'canvas_updates' channel and executes a callback for each message.
     * Runs in a separate detached thread to avoid blocking the main application.
     * @param callback Function to execute when a new message is received.
     */
    void subscribe(std::function<void(const std::string &, const std::string &)> callback)
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

private:
    sw::redis::Redis redis; // Redis client instance

    /**
     * Retrieves the Redis connection URL from environment variables.
     * @return Formatted Redis connection string.
     */
    std::string get_redis_url()
    {
        const char *host = std::getenv("REDIS_HOST");
        return "tcp://" + std::string(host ? host : "redis") + ":6379";
    }
};

#endif
