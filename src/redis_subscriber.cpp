#include "redis_utils.h"
#include "gimbal_position.pb.h"
#include "timestamp.pb.h"
#include <iostream>
#include <thread>
#include <signal.h>

using namespace dp_aero_l2;
using namespace dp_aero_l2::common;

std::atomic<bool> running{true};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down...\n";
    running = false;
}

void subscribe_to_gimbal_positions(redis_utils::RedisMessenger& messenger) {
    std::cout << "Subscribing to gimbal/position channel...\n";
    
    messenger.subscribe<GimbalPosition>("gimbal/position", 
        [](const GimbalPosition& pos) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            
            std::cout << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
                     << "] Received Gimbal Position - Theta: " << pos.theta() 
                     << ", Phi: " << pos.phi() << std::endl;
        });
}

void subscribe_to_timestamps(redis_utils::RedisMessenger& messenger) {
    std::cout << "Subscribing to system/timestamp channel...\n";
    
    messenger.subscribe<Timestamp>("system/timestamp", 
        [](const Timestamp& ts) {
            auto timestamp_ms = ts.timestamp_ms();
            auto time_point = std::chrono::system_clock::from_time_t(timestamp_ms / 1000);
            auto time_t = std::chrono::system_clock::to_time_t(time_point);
            
            std::cout << "[TIMESTAMP] " << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
                     << "." << (timestamp_ms % 1000) << std::endl;
        });
}

void process_queue_messages(redis_utils::RedisMessenger& messenger) {
    std::cout << "Processing queue messages...\n";
    
    while (running) {
        auto message = messenger.pop_from_queue<GimbalPosition>("gimbal_queue", std::chrono::seconds(1));
        if (message) {
            std::cout << "[QUEUE] Processed gimbal position - Theta: " << message->theta() 
                     << ", Phi: " << message->phi() << std::endl;
        }
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <mode>\n";
        std::cout << "Modes:\n";
        std::cout << "  pubsub_gimbal  - Subscribe to gimbal position pub/sub\n";
        std::cout << "  pubsub_time    - Subscribe to timestamp pub/sub\n";
        std::cout << "  queue          - Process queue messages\n";
        return 1;
    }
    
    std::string mode = argv[1];
    
    try {
        redis_utils::RedisMessenger messenger;
        
        if (mode == "pubsub_gimbal") {
            subscribe_to_gimbal_positions(messenger);
        } else if (mode == "pubsub_time") {
            subscribe_to_timestamps(messenger);
        } else if (mode == "queue") {
            process_queue_messages(messenger);
        } else {
            std::cerr << "Unknown mode: " << mode << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}