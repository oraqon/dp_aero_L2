#include "algorithm_strategies.h"
#include "algorithm_framework.h"
#include "task_manager.h"
#include "algorithms/target_tracking_algorithm.h"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace dp_aero_l2::algorithms {

// ============================================================================
// ConfidenceBasedPrioritizer Implementation
// ============================================================================

float ConfidenceBasedPrioritizer::calculate_priority(const Target& target, const fusion::AlgorithmContext& context) const {
    // Simple confidence-based priority
    return target.confidence;
}

std::vector<Target*> ConfidenceBasedPrioritizer::prioritize_targets(std::vector<Target*>& targets, 
                                                                   const fusion::AlgorithmContext& context) const {
    // Sort by confidence (highest first)
    std::sort(targets.begin(), targets.end(), [this, &context](Target* a, Target* b) {
        return calculate_priority(*a, context) > calculate_priority(*b, context);
    });
    
    return targets;
}

Target* ConfidenceBasedPrioritizer::select_highest_priority_target(const std::vector<Target*>& targets,
                                                                  const fusion::AlgorithmContext& context) const {
    if (targets.empty()) return nullptr;
    
    return *std::max_element(targets.begin(), targets.end(), 
        [this, &context](Target* a, Target* b) {
            return calculate_priority(*a, context) < calculate_priority(*b, context);
        });
}

// ============================================================================
// SingleDeviceAssignmentStrategy Implementation  
// ============================================================================

std::string SingleDeviceAssignmentStrategy::select_device_for_target(const Target& target,
                                                                    const fusion::TaskManager& task_manager,
                                                                    const fusion::AlgorithmContext& context) {
    // Always return the single default device
    return default_device_id_;
}

std::string SingleDeviceAssignmentStrategy::select_device_for_task(const Target& target,
                                                                  const std::string& task_type,
                                                                  const fusion::TaskManager& task_manager,
                                                                  const fusion::AlgorithmContext& context) {
    // Always return the single default device regardless of task type
    return default_device_id_;
}

float SingleDeviceAssignmentStrategy::evaluate_device_suitability(const std::string& device_id,
                                                                 const Target& target,
                                                                 const fusion::TaskManager& task_manager,
                                                                 const fusion::AlgorithmContext& context) {
    // If it's our default device, it's perfect. Otherwise, unsuitable.
    return (device_id == default_device_id_) ? 1.0f : 0.0f;
}

// ============================================================================
// CapabilityBasedAssignmentStrategy Implementation
// ============================================================================

CapabilityBasedAssignmentStrategy::CapabilityBasedAssignmentStrategy() {
    // Define which capabilities are needed for each task type
    task_type_to_capabilities_["TRACK_TARGET"] = {"radar", "lidar", "camera", "gimbal_control"};
    task_type_to_capabilities_["SCAN_AREA"] = {"radar", "lidar", "camera"};
    task_type_to_capabilities_["POINT_GIMBAL"] = {"gimbal_control", "coherent"};
    task_type_to_capabilities_["CALIBRATE_SENSOR"] = {"calibration"};
    task_type_to_capabilities_["MONITOR_STATUS"] = {}; // Any device can monitor status
}

std::string CapabilityBasedAssignmentStrategy::select_device_for_target(const Target& target,
                                                                       const fusion::TaskManager& task_manager,
                                                                       const fusion::AlgorithmContext& context) {
    return select_device_for_task(target, "TRACK_TARGET", task_manager, context);
}

std::string CapabilityBasedAssignmentStrategy::select_device_for_task(const Target& target,
                                                                     const std::string& task_type,
                                                                     const fusion::TaskManager& task_manager,
                                                                     const fusion::AlgorithmContext& context) {
    // For now, use hardcoded device evaluation since we don't have device enumeration yet
    std::vector<std::string> candidate_devices = {"default_device", "coherent_001", "radar_001"};
    
    auto best_device_it = std::max_element(candidate_devices.begin(), candidate_devices.end(),
        [this, &target, &task_manager, &context](const std::string& a, const std::string& b) {
            return evaluate_device_suitability(a, target, task_manager, context) < 
                   evaluate_device_suitability(b, target, task_manager, context);
        });
    
    return (best_device_it != candidate_devices.end()) ? *best_device_it : "";
}

float CapabilityBasedAssignmentStrategy::evaluate_device_suitability(const std::string& device_id,
                                                                    const Target& target,
                                                                    const fusion::TaskManager& task_manager,
                                                                    const fusion::AlgorithmContext& context) {
    // Get device capabilities
    auto capabilities = task_manager.get_device_capabilities(device_id);
    
    if (capabilities.empty()) {
        return 0.0f; // Unknown device or no capabilities
    }
    
    // Evaluate based on target characteristics and device capabilities
    float suitability_score = 0.0f;
    
    // Check if device has required capabilities for tracking
    bool has_sensor = std::any_of(capabilities.begin(), capabilities.end(),
        [](const std::string& cap) { 
            return cap == "radar" || cap == "lidar" || cap == "camera"; 
        });
    
    bool has_gimbal = std::any_of(capabilities.begin(), capabilities.end(),
        [](const std::string& cap) { 
            return cap == "gimbal_control" || cap == "coherent"; 
        });
    
    // Base score for having required capabilities
    if (has_sensor) suitability_score += 0.5f;
    if (has_gimbal) suitability_score += 0.5f;
    
    // Bonus for coherent devices if target confidence is high
    bool has_coherent = std::find(capabilities.begin(), capabilities.end(), "coherent") != capabilities.end();
    if (has_coherent && target.confidence > 0.8f) {
        suitability_score += 0.2f; // Prefer coherent for high-confidence targets
    }
    
    return std::min(1.0f, suitability_score);
}

// ============================================================================
// ThreatBasedPrioritizer Implementation
// ============================================================================

float ThreatBasedPrioritizer::calculate_priority(const Target& target, const fusion::AlgorithmContext& context) const {
    float priority = 0.0f;
    
    // Range component (closer = higher threat)
    float range = std::sqrt(target.x * target.x + target.y * target.y + target.z * target.z);
    float range_score = (range > 0) ? std::exp(-range / 100.0f) : 1.0f; // Exponential decay with distance
    priority += params_.range_weight * range_score;
    
    // Velocity component (faster = higher threat)
    float speed = std::sqrt(target.vx * target.vx + target.vy * target.vy + target.vz * target.vz);
    float velocity_score = std::min(1.0f, speed / 50.0f); // Normalize to max speed of 50 m/s
    priority += params_.velocity_weight * velocity_score;
    
    // Confidence component (more confident detections prioritized)
    priority += params_.confidence_weight * target.confidence;
    
    // Heading component (targets moving toward us = higher threat)
    if (range > 0) {
        // Calculate if target is approaching (dot product of velocity and position vectors)
        float approach_factor = -(target.vx * target.x + target.vy * target.y + target.vz * target.z) / (range * speed);
        float heading_score = std::max(0.0f, approach_factor); // Only positive (approaching)
        priority += params_.heading_weight * heading_score;
    }
    
    return std::clamp(priority, 0.0f, 1.0f);
}

std::vector<Target*> ThreatBasedPrioritizer::prioritize_targets(std::vector<Target*>& targets, 
                                                               const fusion::AlgorithmContext& context) const {
    // Sort by threat priority (highest first)
    std::sort(targets.begin(), targets.end(), [this, &context](Target* a, Target* b) {
        return calculate_priority(*a, context) > calculate_priority(*b, context);
    });
    
    return targets;
}

Target* ThreatBasedPrioritizer::select_highest_priority_target(const std::vector<Target*>& targets,
                                                              const fusion::AlgorithmContext& context) const {
    if (targets.empty()) return nullptr;
    
    auto best_target = *std::max_element(targets.begin(), targets.end(), 
        [this, &context](Target* a, Target* b) {
            return calculate_priority(*a, context) < calculate_priority(*b, context);
        });
    
    std::cout << "[ThreatBasedPrioritizer] Selected target with threat priority: " 
              << calculate_priority(*best_target, context) << std::endl;
    
    return best_target;
}

} // namespace dp_aero_l2::algorithms