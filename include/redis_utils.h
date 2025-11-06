#pragma once

#include <sw/redis++/redis++.h>
#include <google/protobuf/message.h>
#include <string>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>

namespace dp_aero_l2 {
namespace redis_utils {

using namespace sw::redis;

class RedisMessenger {
private:
    mutable std::mutex redis_mutex_;  // Protect Redis operations
    
public:
    explicit RedisMessenger(const std::string& redis_url = "tcp://127.0.0.1:6379")
        : redis_(redis_url) {}

    // Get subscriber for controlled lifecycle management
    sw::redis::Subscriber get_subscriber() {
        std::lock_guard<std::mutex> lock(redis_mutex_);
        return redis_.subscriber();
    }

    // Serialize protobuf message to string
    template<typename T>
    std::string serialize_message(const T& message) {
        std::string serialized;
        if (!message.SerializeToString(&serialized)) {
            throw std::runtime_error("Failed to serialize protobuf message");
        }
        return serialized;
    }

    // Deserialize string to protobuf message
    template<typename T>
    T deserialize_message(const std::string& data) {
        T message;
        if (!message.ParseFromString(data)) {
            throw std::runtime_error("Failed to deserialize protobuf message");
        }
        return message;
    }

    // Publish message using Redis Pub/Sub
    template<typename T>
    void publish(const std::string& channel, const T& message) {
        std::lock_guard<std::mutex> lock(redis_mutex_);
        auto serialized = serialize_message(message);
        redis_.publish(channel, serialized);
    }

    // Subscribe to channel with callback (with shutdown support)
    template<typename T>
    void subscribe(const std::string& channel, 
                  std::function<void(const T&)> callback,
                  const std::atomic<bool>* shutdown_flag = nullptr) {
        auto subscriber = redis_.subscriber();
        subscriber.on_message([this, callback](std::string channel, std::string msg) {
            try {
                auto message = deserialize_message<T>(msg);
                callback(message);
            } catch (const std::exception& e) {
                std::cerr << "Error deserializing message: " << e.what() << std::endl;
            }
        });
        
        subscriber.subscribe(channel);
        while (true) {
            // Check shutdown flag frequently
            if (shutdown_flag && !(*shutdown_flag)) {
                break;
            }
            
            try {
                subscriber.consume();
            } catch (const Error& e) {
                std::cerr << "Redis subscription error: " << e.what() << std::endl;
                break;
            }
        }
    }

    // Add message to Redis Stream
    template<typename T>
    std::string add_to_stream(const std::string& stream_name, const T& message) {
        std::lock_guard<std::mutex> lock(redis_mutex_);
        auto serialized = serialize_message(message);
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        
        std::unordered_map<std::string, std::string> fields = {
            {"data", serialized},
            {"timestamp", std::to_string(timestamp)}
        };
        
        return redis_.xadd(stream_name, "*", fields.begin(), fields.end());
    }

    // Read from Redis Stream (TODO: Fix redis++ stream reading)
    template<typename T>
    std::vector<std::pair<std::string, T>> read_from_stream(
        const std::string& stream_name, 
        const std::string& start_id = "0",
        size_t count = 10) {
        
        std::vector<std::pair<std::string, T>> results;
        // TODO: Implement stream reading with correct redis++ API
        return results;
    }

    // Push to Redis List (FIFO queue)
    template<typename T>
    void push_to_queue(const std::string& queue_name, const T& message) {
        std::lock_guard<std::mutex> lock(redis_mutex_);
        auto serialized = serialize_message(message);
        redis_.lpush(queue_name, serialized);
    }

    // Pop from Redis List (FIFO queue)
    template<typename T>
    std::optional<T> pop_from_queue(const std::string& queue_name, 
                                   std::chrono::seconds timeout = std::chrono::seconds(1)) {
        std::lock_guard<std::mutex> lock(redis_mutex_);
        try {
            auto result = redis_.brpop(queue_name, timeout);
            if (result) {
                return deserialize_message<T>(result->second);
            }
        } catch (const Error& e) {
            std::cerr << "Redis queue pop error: " << e.what() << std::endl;
        }
        return std::nullopt;
    }

private:
    Redis redis_;
};

} // namespace redis_utils
} // namespace dp_aero_l2