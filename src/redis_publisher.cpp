#include "redis_utils.h"
#include "common/gimbal_position.pb.h"
#include "common/timestamp.pb.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

using namespace dp_aero_l2;
using namespace dp_aero_l2::common;

int main() {
    try {
        redis_utils::RedisMessenger messenger;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<float> theta_dist(-M_PI, M_PI);  // Full rotation
        std::uniform_real_distribution<float> phi_dist(-M_PI/2, M_PI/2); // Elevation range
        
        std::cout << "Starting Redis Publisher - Publishing gimbal positions every second\n";
        std::cout << "Press Ctrl+C to stop\n\n";
        
        int message_count = 0;
        
        while (true) {
            // Create gimbal position message
            GimbalPosition gimbal_pos;
            gimbal_pos.set_theta(theta_dist(gen));
            gimbal_pos.set_phi(phi_dist(gen));
            
            // Create timestamp
            Timestamp timestamp;
            auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            timestamp.set_timestamp_ms(now);
            
            // Publish via Pub/Sub
            messenger.publish("gimbal/position", gimbal_pos);
            messenger.publish("system/timestamp", timestamp);
            
            // Also add to stream for persistent storage
            auto stream_id = messenger.add_to_stream("gimbal_stream", gimbal_pos);
            
            // Add to queue for worker processes
            messenger.push_to_queue("gimbal_queue", gimbal_pos);
            
            message_count++;
            std::cout << "Published message #" << message_count 
                     << " - Theta: " << gimbal_pos.theta() 
                     << ", Phi: " << gimbal_pos.phi()
                     << " (Stream ID: " << stream_id << ")\n";
            
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}