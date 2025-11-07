#include "algorithms/target_tracking_algorithm.h"
#include "algorithm_strategies.h"
#include <iostream>

using namespace dp_aero_l2;

/**
 * @brief Demonstration of different algorithm strategies
 */
void demonstrate_strategy_usage() {
    std::cout << "=== Algorithm Strategy Demonstration ===" << std::endl;
    
    // Create target tracking algorithm
    algorithms::TargetTrackingAlgorithm algorithm;
    
    // Demonstrate different target prioritizers
    std::cout << "\n1. Using ConfidenceBasedPrioritizer (default):" << std::endl;
    algorithm.set_target_prioritizer(std::make_unique<algorithms::ConfidenceBasedPrioritizer>());
    
    std::cout << "   - Prioritizes targets by confidence score" << std::endl;
    std::cout << "   - Simple and reliable for most scenarios" << std::endl;
    
    std::cout << "\n2. Switching to ThreatBasedPrioritizer:" << std::endl;
    algorithm.set_target_prioritizer(std::make_unique<algorithms::ThreatBasedPrioritizer>());
    
    std::cout << "   - Considers range, velocity, heading, confidence" << std::endl;
    std::cout << "   - Closer, faster, approaching targets get higher priority" << std::endl;
    
    // Demonstrate device assignment strategies
    std::cout << "\n3. Using SingleDeviceAssignmentStrategy (current demo):" << std::endl;
    algorithm.set_device_assignment_strategy(
        std::make_unique<algorithms::SingleDeviceAssignmentStrategy>("default_device"));
    
    std::cout << "   - All targets assigned to single device" << std::endl;
    std::cout << "   - Perfect for first demo with one device" << std::endl;
    
    std::cout << "\n4. Ready for CapabilityBasedAssignmentStrategy (future):" << std::endl;
    algorithm.set_device_assignment_strategy(
        std::make_unique<algorithms::CapabilityBasedAssignmentStrategy>());
    
    std::cout << "   - Matches device capabilities to target requirements" << std::endl;
    std::cout << "   - Optimal for multi-device scenarios" << std::endl;
    
    std::cout << "\n=== Strategy Architecture Benefits ===" << std::endl;
    std::cout << "✓ Modular: Can swap algorithms independently" << std::endl;
    std::cout << "✓ Extensible: Easy to add new prioritization/assignment logic" << std::endl;
    std::cout << "✓ Testable: Each strategy can be unit tested separately" << std::endl;
    std::cout << "✓ Configurable: Runtime selection of strategies" << std::endl;
    
    std::cout << "\n=== Usage Patterns ===" << std::endl;
    std::cout << "// Simple confidence-based (current demo)" << std::endl;
    std::cout << "algorithm.set_target_prioritizer(std::make_unique<ConfidenceBasedPrioritizer>());" << std::endl;
    
    std::cout << "\n// Advanced threat assessment (future)" << std::endl;
    std::cout << "auto threat_params = ThreatBasedPrioritizer::ThreatParameters{" << std::endl;
    std::cout << "    .range_weight = 0.4f, .velocity_weight = 0.3f" << std::endl;
    std::cout << "};" << std::endl;
    std::cout << "algorithm.set_target_prioritizer(std::make_unique<ThreatBasedPrioritizer>(threat_params));" << std::endl;
}

int main() {
    demonstrate_strategy_usage();
    return 0;
}