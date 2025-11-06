#include "redis_utils.h"
#include "common/gimbal_position.pb.h"
#include "common/timestamp.pb.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace dp_aero_l2;
using namespace dp_aero_l2::common;

void demonstrate_stream_operations(redis_utils::RedisMessenger& messenger) {
    std::cout << "=== Redis Streams Example ===\n\n";
    
    // Read existing messages from stream
    std::cout << "Reading existing messages from gimbal_stream:\n";
    auto existing_messages = messenger.read_from_stream<GimbalPosition>("gimbal_stream", "0", 5);
    
    if (existing_messages.empty()) {
        std::cout << "No existing messages found in stream.\n";
    } else {
        for (const auto& [id, pos] : existing_messages) {
            std::cout << "Stream ID: " << id 
                     << " - Theta: " << pos.theta() 
                     << ", Phi: " << pos.phi() << std::endl;
        }
    }
    
    std::cout << "\n=== Continuous Stream Reading ===\n";
    std::cout << "Reading new messages from gimbal_stream (press Ctrl+C to stop)...\n\n";
    
    std::string last_id = "$";  // Start from latest messages
    
    while (true) {
        auto new_messages = messenger.read_from_stream<GimbalPosition>("gimbal_stream", last_id, 10);
        
        for (const auto& [id, pos] : new_messages) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            
            std::cout << "[" << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
                     << "] Stream ID: " << id 
                     << " - Theta: " << pos.theta() 
                     << ", Phi: " << pos.phi() << std::endl;
            
            last_id = id;
        }
        
        if (new_messages.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
}

void demonstrate_stream_analytics(redis_utils::RedisMessenger& messenger) {
    std::cout << "=== Stream Analytics Example ===\n\n";
    
    // Read last 20 messages for analysis
    auto messages = messenger.read_from_stream<GimbalPosition>("gimbal_stream", "0", 20);
    
    if (messages.empty()) {
        std::cout << "No messages found for analysis.\n";
        return;
    }
    
    // Calculate statistics
    float sum_theta = 0.0f, sum_phi = 0.0f;
    float min_theta = messages[0].second.theta(), max_theta = messages[0].second.theta();
    float min_phi = messages[0].second.phi(), max_phi = messages[0].second.phi();
    
    for (const auto& [id, pos] : messages) {
        sum_theta += pos.theta();
        sum_phi += pos.phi();
        
        min_theta = std::min(min_theta, pos.theta());
        max_theta = std::max(max_theta, pos.theta());
        min_phi = std::min(min_phi, pos.phi());
        max_phi = std::max(max_phi, pos.phi());
    }
    
    float avg_theta = sum_theta / messages.size();
    float avg_phi = sum_phi / messages.size();
    
    std::cout << "Analysis of " << messages.size() << " gimbal positions:\n";
    std::cout << "Theta (Azimuth):\n";
    std::cout << "  Average: " << avg_theta << " rad\n";
    std::cout << "  Range: " << min_theta << " to " << max_theta << " rad\n";
    std::cout << "Phi (Elevation):\n";
    std::cout << "  Average: " << avg_phi << " rad\n";
    std::cout << "  Range: " << min_phi << " to " << max_phi << " rad\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <mode>\n";
        std::cout << "Modes:\n";
        std::cout << "  read      - Continuously read from stream\n";
        std::cout << "  analytics - Analyze existing stream data\n";
        return 1;
    }
    
    std::string mode = argv[1];
    
    try {
        redis_utils::RedisMessenger messenger;
        
        if (mode == "read") {
            demonstrate_stream_operations(messenger);
        } else if (mode == "analytics") {
            demonstrate_stream_analytics(messenger);
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