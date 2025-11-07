#pragma once

#include "target.h"
#include "algorithm_framework.h"
#include <vector>
#include <memory>
#include <cmath>
#include <string>
#include <chrono>

using namespace dp_aero_l2::algorithms;
using namespace dp_aero_l2::fusion;

/**
 * @brief Factory for creating test Target objects with various characteristics
 */
class TargetFactory {
public:
    /**
     * @brief Create a target with high confidence
     */
    static Target createHighConfidenceTarget() {
        Target target;
        target.target_id = "high_conf_001";
        target.x = 100.0f;
        target.y = 200.0f;
        target.z = 50.0f;
        target.vx = -10.0f;  // Moving toward origin
        target.vy = -20.0f;
        target.vz = 0.0f;
        target.confidence = 0.95f;
        target.last_update = std::chrono::steady_clock::now();
        return target;
    }
    
    /**
     * @brief Create a target with low confidence
     */
    static Target createLowConfidenceTarget() {
        Target target;
        target.target_id = "low_conf_001";
        target.x = 500.0f;
        target.y = 300.0f;
        target.z = 100.0f;
        target.vx = 5.0f;   // Moving away from origin
        target.vy = 8.0f;
        target.vz = 2.0f;
        target.confidence = 0.25f;
        target.last_update = std::chrono::steady_clock::now();
        return target;
    }
    
    /**
     * @brief Create a target that's approaching rapidly (high threat)
     */
    static Target createApproachingTarget() {
        Target target;
        target.target_id = "approaching_001";
        target.x = 50.0f;   // Close range
        target.y = 30.0f;
        target.z = 10.0f;
        target.vx = -25.0f;  // High velocity toward origin
        target.vy = -15.0f;
        target.vz = -5.0f;
        target.confidence = 0.85f;
        target.last_update = std::chrono::steady_clock::now();
        return target;
    }
    
    /**
     * @brief Create a distant, slow target (low threat)
     */
    static Target createDistantTarget() {
        Target target;
        target.target_id = "distant_001";
        target.x = 1000.0f;  // Far away
        target.y = 800.0f;
        target.z = 200.0f;
        target.vx = 2.0f;    // Slow velocity
        target.vy = 1.0f;
        target.vz = 0.5f;
        target.confidence = 0.70f;
        target.last_update = std::chrono::steady_clock::now();
        return target;
    }
    
    /**
     * @brief Create a cluster of targets with varying properties
     */
    static std::vector<Target> createTargetCluster() {
        return {
            createHighConfidenceTarget(),
            createLowConfidenceTarget(),
            createApproachingTarget(),
            createDistantTarget()
        };
    }
    
    /**
     * @brief Create targets with specific confidence values for sorting tests
     */
    static std::vector<Target> createSortableTargets() {
        std::vector<Target> targets;
        
        // Create targets with different confidence levels
        for (int i = 0; i < 5; ++i) {
            Target target;
            target.target_id = "sortable_" + std::to_string(i);
            target.x = 100.0f + i * 10.0f;
            target.y = 100.0f;
            target.z = 0.0f;
            target.vx = 0.0f;
            target.vy = 0.0f;
            target.vz = 0.0f;
            target.confidence = 0.2f + (i * 0.2f);  // 0.2, 0.4, 0.6, 0.8, 1.0
            target.last_update = std::chrono::steady_clock::now();
            targets.push_back(target);
        }
        
        return targets;
    }
};

/**
 * @brief Factory for creating device capability configurations
 */
class DeviceCapabilityFactory {
public:
    static std::vector<std::string> createRadarCapabilities() {
        return {"radar", "long_range_detection", "weather_resistant"};
    }
    
    static std::vector<std::string> createCoherentCapabilities() {
        return {"coherent", "gimbal_control", "high_precision_targeting"};
    }
    
    static std::vector<std::string> createLidarCapabilities() {
        return {"lidar", "high_resolution_mapping", "close_range_detection"};
    }
    
    static std::vector<std::string> createMultiModalCapabilities() {
        return {"radar", "lidar", "camera", "gimbal_control", "coherent"};
    }
    
    static std::vector<std::string> createBasicCapabilities() {
        return {"basic_sensor", "status_monitoring"};
    }
};

/**
 * @brief Mock Algorithm Context for testing
 */
class MockAlgorithmContext : public AlgorithmContext {
private:
    std::unordered_map<std::string, std::any> test_data_;
    
public:
    MockAlgorithmContext() {
        current_state_name = "IDLE";
        // Pre-populate with common test data
        test_data_["test_mode"] = true;
        test_data_["simulation_time"] = std::chrono::steady_clock::now();
    }
    
    // Override data access for predictable testing
    template<typename T>
    void set_test_data(const std::string& key, const T& value) {
        test_data_[key] = value;
    }
    
    template<typename T>
    std::optional<T> get_test_data(const std::string& key) const {
        auto it = test_data_.find(key);
        if (it != test_data_.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
    
    // Helper to clear test data between tests
    void clear_test_data() {
        test_data_.clear();
    }
};

/**
 * @brief Test utilities for common assertions
 */
class TestUtils {
public:
    /**
     * @brief Assert that targets are sorted by confidence (descending)
     */
    static void assertTargetsSortedByConfidence(const std::vector<Target*>& targets) {
        for (size_t i = 1; i < targets.size(); ++i) {
            if (targets[i-1]->confidence < targets[i]->confidence) {
                throw std::runtime_error("Targets not sorted by confidence descending");
            }
        }
    }
    
    /**
     * @brief Calculate distance between two targets
     */
    static float calculateDistance(const Target& a, const Target& b) {
        float dx = a.x - b.x;
        float dy = a.y - b.y;
        float dz = a.z - b.z;
        return std::sqrt(dx*dx + dy*dy + dz*dz);
    }
    
    /**
     * @brief Check if target is approaching origin
     */
    static bool isApproachingOrigin(const Target& target) {
        // Calculate dot product of position and velocity vectors
        float dot_product = target.x * target.vx + target.y * target.vy + target.z * target.vz;
        return dot_product < 0;  // Negative means approaching
    }
};