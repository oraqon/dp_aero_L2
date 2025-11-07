#pragma once

#include "algorithm_framework.h"
#include "algorithm_strategies.h"
#include <shared_mutex>

namespace dp_aero_l2::fusion {

/**
 * @brief Enhanced fusion algorithm with strategy support
 */
class StrategyBasedFusionAlgorithm : public FusionAlgorithm {
protected:
    // Strategy interfaces for modular algorithm components
    std::unique_ptr<algorithms::TargetPrioritizer> target_prioritizer_;
    std::unique_ptr<algorithms::DeviceAssignmentStrategy> device_assignment_strategy_;
    
    // Thread safety for strategy access
    mutable std::shared_mutex strategy_mutex_;
    
public:
    virtual ~StrategyBasedFusionAlgorithm() = default;
    
    /**
     * @brief Set target prioritization strategy (thread-safe)
     */
    void set_target_prioritizer(std::unique_ptr<algorithms::TargetPrioritizer> prioritizer) {
        std::unique_lock lock(strategy_mutex_);
        target_prioritizer_ = std::move(prioritizer);
    }
    
    /**
     * @brief Set device assignment strategy (thread-safe)
     */
    void set_device_assignment_strategy(std::unique_ptr<algorithms::DeviceAssignmentStrategy> strategy) {
        std::unique_lock lock(strategy_mutex_);
        device_assignment_strategy_ = std::move(strategy);
    }
    
    /**
     * @brief Get target prioritizer (for use by algorithms) - thread-safe
     */
    algorithms::TargetPrioritizer* get_target_prioritizer() const {
        std::shared_lock lock(strategy_mutex_);
        return target_prioritizer_.get();
    }
    
    /**
     * @brief Get device assignment strategy (for use by algorithms) - thread-safe
     */
    algorithms::DeviceAssignmentStrategy* get_device_assignment_strategy() const {
        std::shared_lock lock(strategy_mutex_);
        return device_assignment_strategy_.get();
    }
    
    /**
     * @brief Safe strategy access with RAII guard
     * Usage: with_target_prioritizer([&](auto& prioritizer) { return prioritizer.calculate_priority(target, context); })
     */
    template<typename Func>
    auto with_target_prioritizer(Func&& func) const -> decltype(func(*target_prioritizer_)) {
        std::shared_lock lock(strategy_mutex_);
        if (target_prioritizer_) {
            return func(*target_prioritizer_);
        }
        throw std::runtime_error("No target prioritizer set");
    }
    
    /**
     * @brief Safe device assignment strategy access with RAII guard
     */
    template<typename Func>
    auto with_device_assignment_strategy(Func&& func) const -> decltype(func(*device_assignment_strategy_)) {
        std::shared_lock lock(strategy_mutex_);
        if (device_assignment_strategy_) {
            return func(*device_assignment_strategy_);
        }
        throw std::runtime_error("No device assignment strategy set");
    }
};

} // namespace dp_aero_l2::fusion