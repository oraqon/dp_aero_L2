#pragma once

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include "target.h"

// Forward declarations
namespace dp_aero_l2::fusion {
    class AlgorithmContext;
    class TaskManager;
}

namespace dp_aero_l2::algorithms {

/**
 * @brief Abstract interface for target prioritization strategies
 */
class TargetPrioritizer {
public:
    virtual ~TargetPrioritizer() = default;
    
    /**
     * @brief Calculate priority score for a target
     * @param target Target to prioritize
     * @param context Algorithm context for additional data
     * @return Priority score (higher = more important)
     */
    virtual float calculate_priority(const Target& target, const fusion::AlgorithmContext& context) const = 0;
    
        /**
     * @brief Prioritize targets by sorting them
     * @param targets Vector of target pointers (will be modified)
     * @param context Algorithm context for additional data
     * @return Sorted vector (highest priority first)
     */
    virtual std::vector<Target*> prioritize_targets(std::vector<Target*>& targets, 
                                                   const fusion::AlgorithmContext& context) const = 0;
    
    /**
     * @brief Select the single highest priority target from a list
     * @param targets Vector of target pointers
     * @param context Algorithm context for additional data
     * @return Pointer to highest priority target, or nullptr if none
     */
    virtual Target* select_highest_priority_target(const std::vector<Target*>& targets,
                                                  const fusion::AlgorithmContext& context) const = 0;    /**
     * @brief Get prioritizer name for logging/debugging
     */
    virtual std::string get_name() const = 0;
};

/**
 * @brief Abstract interface for device assignment strategies
 */
class DeviceAssignmentStrategy {
public:
    virtual ~DeviceAssignmentStrategy() = default;
    
    /**
     * @brief Select the best device for a target
     * @param target Target that needs device assignment
     * @param task_manager Task manager with device capabilities and availability
     * @param context Algorithm execution context
     * @return Device ID to assign, or empty string if no suitable device
     */
    virtual std::string select_device_for_target(const Target& target,
                                                const fusion::TaskManager& task_manager,
                                                const fusion::AlgorithmContext& context) = 0;
    
    /**
     * @brief Select device for a specific task type
     * @param target Target associated with the task
     * @param task_type Type of task to be performed
     * @param task_manager Task manager with device information
     * @param context Algorithm execution context
     * @return Device ID to assign, or empty string if no suitable device
     */
    virtual std::string select_device_for_task(const Target& target,
                                              const std::string& task_type,
                                              const fusion::TaskManager& task_manager,
                                              const fusion::AlgorithmContext& context) = 0;
    
    /**
     * @brief Evaluate if a device can handle a target
     * @param device_id Device to evaluate
     * @param target Target to be handled
     * @param task_manager Task manager with device capabilities
     * @param context Algorithm execution context
     * @return Score (0.0 = cannot handle, 1.0 = perfect match)
     */
    virtual float evaluate_device_suitability(const std::string& device_id,
                                             const Target& target,
                                             const fusion::TaskManager& task_manager,
                                             const fusion::AlgorithmContext& context) = 0;
    
    /**
     * @brief Get strategy name for logging/debugging
     */
    virtual std::string get_name() const = 0;
};

// ============================================================================
// DEFAULT IMPLEMENTATIONS
// ============================================================================

/**
 * @brief Default confidence-based target prioritizer
 */
class ConfidenceBasedPrioritizer : public TargetPrioritizer {
public:
    float calculate_priority(const Target& target, const fusion::AlgorithmContext& context) const override;
    
    std::vector<Target*> prioritize_targets(std::vector<Target*>& targets, 
                                          const fusion::AlgorithmContext& context) const override;
    
    Target* select_highest_priority_target(const std::vector<Target*>& targets,
                                         const fusion::AlgorithmContext& context) const override;
    
    std::string get_name() const override { return "ConfidenceBasedPrioritizer"; }
};

/**
 * @brief Default single-device assignment strategy (for first demo)
 */
class SingleDeviceAssignmentStrategy : public DeviceAssignmentStrategy {
private:
    std::string default_device_id_;
    
public:
    explicit SingleDeviceAssignmentStrategy(const std::string& device_id) 
        : default_device_id_(device_id) {}
    
    std::string select_device_for_target(const Target& target,
                                       const fusion::TaskManager& task_manager,
                                       const fusion::AlgorithmContext& context) override;
    
    std::string select_device_for_task(const Target& target,
                                     const std::string& task_type,
                                     const fusion::TaskManager& task_manager,
                                     const fusion::AlgorithmContext& context) override;
    
    float evaluate_device_suitability(const std::string& device_id,
                                     const Target& target,
                                     const fusion::TaskManager& task_manager,
                                     const fusion::AlgorithmContext& context) override;
    
    std::string get_name() const override { return "SingleDeviceAssignmentStrategy"; }
};

/**
 * @brief Capability-based device assignment strategy (for multi-device scenarios)
 */
class CapabilityBasedAssignmentStrategy : public DeviceAssignmentStrategy {
private:
    std::unordered_map<std::string, std::vector<std::string>> task_type_to_capabilities_;
    
public:
    CapabilityBasedAssignmentStrategy();
    
    std::string select_device_for_target(const Target& target,
                                       const fusion::TaskManager& task_manager,
                                       const fusion::AlgorithmContext& context) override;
    
    std::string select_device_for_task(const Target& target,
                                     const std::string& task_type,
                                     const fusion::TaskManager& task_manager,
                                     const fusion::AlgorithmContext& context) override;
    
    float evaluate_device_suitability(const std::string& device_id,
                                     const Target& target,
                                     const fusion::TaskManager& task_manager,
                                     const fusion::AlgorithmContext& context) override;
    
    std::string get_name() const override { return "CapabilityBasedAssignmentStrategy"; }
};

/**
 * @brief Threat-based target prioritizer
 */
class ThreatBasedPrioritizer : public TargetPrioritizer {
public:
    struct ThreatParameters {
        float range_weight = 0.3f;      // Closer targets are higher threat
        float velocity_weight = 0.2f;   // Faster targets are higher threat  
        float confidence_weight = 0.3f; // More confident detections prioritized
        float heading_weight = 0.2f;    // Targets heading toward us are higher threat
    };

private:
    
    ThreatParameters params_;
    
public:
    ThreatBasedPrioritizer() : params_{} {}
    explicit ThreatBasedPrioritizer(const ThreatParameters& params) : params_(params) {}
    
    float calculate_priority(const Target& target, const fusion::AlgorithmContext& context) const override;
    
    std::vector<Target*> prioritize_targets(std::vector<Target*>& targets, 
                                          const fusion::AlgorithmContext& context) const override;
    
    Target* select_highest_priority_target(const std::vector<Target*>& targets,
                                         const fusion::AlgorithmContext& context) const override;
    
    std::string get_name() const override { return "ThreatBasedPrioritizer"; }
    
    void set_parameters(const ThreatParameters& params) { params_ = params; }
    const ThreatParameters& get_parameters() const { return params_; }
};

} // namespace dp_aero_l2::algorithms