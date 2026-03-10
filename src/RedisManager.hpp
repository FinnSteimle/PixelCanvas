#ifndef REDISMANAGER_HPP
#define REDISMANAGER_HPP

#include <sw/redis++/redis++.h>
#include <iostream>
#include <string>
#include <thread>
#include <functional>

class RedisManager
{
public:
    // Takes a callback function so it knows how to broadcast to Crow's WebSockets
    RedisManager(std::function<void(const std::string &)> onMessageCallback)
        : redis("tcp://127.0.0.1:6379"), callback(onMessageCallback)
    {
        // Redis Subscriptions block the thread, so we run it in the background
        sub_thread = std::thread([this]()
                                 {
            auto sub = redis.subscriber();
            
            sub.on_message([this](std::string channel, std::string msg) {
                if (channel == "canvas_updates") {
                    callback(msg); // Trigger the broadcast to users
                }
            });
            
            sub.subscribe("canvas_updates");
            std::cout << "Subscribed to Redis 'canvas_updates' channel." << std::endl;

            while (true) {
                try {
                    sub.consume(); // Blocks until a message arrives
                } catch (const sw::redis::Error& e) {
                    std::cerr << "Redis Error: " << e.what() << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
            } });

        sub_thread.detach(); // Let the thread run independently
    }

    void publish(const std::string &message)
    {
        redis.publish("canvas_updates", message);
    }

private:
    sw::redis::Redis redis;
    std::thread sub_thread;
    std::function<void(const std::string &)> callback;
};

#endif