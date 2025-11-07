#pragma once

#include <string>
#include <chrono>
#include <unordered_map>

namespace dp_aero_l2::algorithms {

/**
 * @brief Target representation for tracking algorithms
 */
struct Target {
    std::string target_id;
    float x, y, z;          // Position
    float vx, vy, vz;       // Velocity
    float confidence;       // Confidence score
    std::chrono::steady_clock::time_point last_update;
    std::unordered_map<std::string, int> sensor_detections; // Count per sensor
    
    // Default constructor (required for std::unordered_map)
    Target() : target_id(""), x(0), y(0), z(0), vx(0), vy(0), vz(0), confidence(0) {}
    
    // Parameterized constructor
    Target(const std::string& id) : target_id(id), x(0), y(0), z(0), 
                                   vx(0), vy(0), vz(0), confidence(0) {}
};

} // namespace dp_aero_l2::algorithms