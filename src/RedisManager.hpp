#ifndef REDISMANAGER_HPP
#define REDISMANAGER_HPP

#include <sw/redis++/redis++.h>
#include <string>
#include <functional>

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
    RedisManager();

    /**
     * Publishes a pixel update to the 'canvas_updates' channel.
     * @param x X-coordinate of the pixel.
     * @param y Y-coordinate of the pixel.
     * @param color Hex color string of the pixel.
     */
    void publishPixel(int x, int y, const std::string &color);

    /**
     * Subscribes to the 'canvas_updates' channel and executes a callback for each message.
     * Runs in a separate detached thread to avoid blocking the main application.
     * @param callback Function to execute when a new message is received.
     */
    void subscribe(std::function<void(const std::string &, const std::string &)> callback);

private:
    sw::redis::Redis redis; // Redis client instance

    /**
     * Retrieves the Redis connection URL from environment variables.
     * @return Formatted Redis connection string.
     */
    std::string get_redis_url();
};

#endif