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
    RedisManager() : redis(get_redis_url())
    {
        std::cout << "Connected to Redis at " << get_redis_url() << std::endl;
    }

    void publishPixel(int x, int y, const std::string &color)
    {
        nlohmann::json j;
        j["x"] = x;
        j["y"] = y;
        j["color"] = color;
        redis.publish("canvas_updates", j.dump());
    }

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
                    while (true) {
                        sub.consume();
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Redis Error: " << e.what() << ". Retrying..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                }
            } })
            .detach();
    }

private:
    sw::redis::Redis redis;

    std::string get_redis_url()
    {
        const char *host = std::getenv("REDIS_HOST");
        return "tcp://" + std::string(host ? host : "redis") + ":6379";
    }
};

#endif
