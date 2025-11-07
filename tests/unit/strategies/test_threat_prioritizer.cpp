#include <gtest/gtest.h>
#include "algorithm_strategies.h"
#include "test_data_factory.h"
#include <cmath>

using namespace dp_aero_l2::algorithms;
using namespace dp_aero_l2::fusion;

/**
 * @brief Test fixture for ThreatBasedPrioritizer
 */
class ThreatBasedPrioritizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Use default threat parameters
        prioritizer = std::make_unique<ThreatBasedPrioritizer>();
        context = std::make_unique<MockAlgorithmContext>();
        
        // Custom parameters for specific tests
        ThreatBasedPrioritizer::ThreatParameters custom_params;
        custom_params.range_weight = 0.4f;
        custom_params.velocity_weight = 0.3f; 
        custom_params.confidence_weight = 0.2f;
        custom_params.heading_weight = 0.1f;
        
        custom_prioritizer = std::make_unique<ThreatBasedPrioritizer>(custom_params);
    }
    
    void TearDown() override {
        prioritizer.reset();
        custom_prioritizer.reset();
        context.reset();
    }
    
    std::unique_ptr<ThreatBasedPrioritizer> prioritizer;
    std::unique_ptr<ThreatBasedPrioritizer> custom_prioritizer;
    std::unique_ptr<MockAlgorithmContext> context;
};

/**
 * @brief Test that closer targets get higher priority
 */
TEST_F(ThreatBasedPrioritizerTest, PrioritizesCloserTargets) {
    auto close_target = TargetFactory::createApproachingTarget();  // Close range
    auto distant_target = TargetFactory::createDistantTarget();    // Far range
    
    // Both should have same confidence to isolate range factor
    close_target.confidence = 0.8f;
    distant_target.confidence = 0.8f;
    
    // Both stationary to isolate range factor
    close_target.vx = close_target.vy = close_target.vz = 0.0f;
    distant_target.vx = distant_target.vy = distant_target.vz = 0.0f;
    
    float close_priority = prioritizer->calculate_priority(close_target, *context);
    float distant_priority = prioritizer->calculate_priority(distant_target, *context);
    
    EXPECT_GT(close_priority, distant_priority) 
        << "Closer target should have higher threat priority";
}

/**
 * @brief Test that faster targets get higher priority
 */
TEST_F(ThreatBasedPrioritizerTest, PrioritizesFasterTargets) {
    auto fast_target = TargetFactory::createApproachingTarget();  // High velocity
    auto slow_target = TargetFactory::createDistantTarget();      // Low velocity
    
    // Same position and confidence to isolate velocity factor
    fast_target.x = slow_target.x = 100.0f;
    fast_target.y = slow_target.y = 100.0f; 
    fast_target.z = slow_target.z = 50.0f;
    fast_target.confidence = slow_target.confidence = 0.8f;
    
    float fast_priority = prioritizer->calculate_priority(fast_target, *context);
    float slow_priority = prioritizer->calculate_priority(slow_target, *context);
    
    EXPECT_GT(fast_priority, slow_priority)
        << "Faster target should have higher threat priority";
}

/**
 * @brief Test that approaching targets get higher priority than receding ones
 */
TEST_F(ThreatBasedPrioritizerTest, PrioritizesApproachingTargets) {
    Target approaching_target = TargetFactory::createApproachingTarget();
    
    // Create receding target (same position, opposite velocity)
    Target receding_target = approaching_target;
    receding_target.target_id = "receding_001";
    receding_target.vx = -approaching_target.vx;  // Reverse velocity
    receding_target.vy = -approaching_target.vy;
    receding_target.vz = -approaching_target.vz;
    
    float approaching_priority = prioritizer->calculate_priority(approaching_target, *context);
    float receding_priority = prioritizer->calculate_priority(receding_target, *context);
    
    EXPECT_GT(approaching_priority, receding_priority)
        << "Approaching target should have higher priority than receding target";
}

/**
 * @brief Test confidence factor contribution
 */
TEST_F(ThreatBasedPrioritizerTest, ConsidersConfidenceFactor) {
    auto high_conf = TargetFactory::createHighConfidenceTarget();
    auto low_conf = TargetFactory::createLowConfidenceTarget();
    
    // Same position and velocity to isolate confidence factor
    low_conf.x = high_conf.x;
    low_conf.y = high_conf.y;
    low_conf.z = high_conf.z;
    low_conf.vx = high_conf.vx;
    low_conf.vy = high_conf.vy;
    low_conf.vz = high_conf.vz;
    
    float high_conf_priority = prioritizer->calculate_priority(high_conf, *context);
    float low_conf_priority = prioritizer->calculate_priority(low_conf, *context);
    
    EXPECT_GT(high_conf_priority, low_conf_priority)
        << "Higher confidence target should have higher threat priority";
}

/**
 * @brief Test priority bounds (should be clamped to [0,1])
 */
TEST_F(ThreatBasedPrioritizerTest, ClampsPriorityToBounds) {
    // Create extreme target that might exceed bounds
    Target extreme_target;
    extreme_target.target_id = "extreme_001";
    extreme_target.x = 1.0f;     // Very close
    extreme_target.y = 1.0f;
    extreme_target.z = 1.0f;
    extreme_target.vx = -100.0f;  // Very fast approaching
    extreme_target.vy = -100.0f;
    extreme_target.vz = -100.0f;
    extreme_target.confidence = 1.0f;  // Maximum confidence
    
    float priority = prioritizer->calculate_priority(extreme_target, *context);
    
    EXPECT_GE(priority, 0.0f) << "Priority should not be below 0.0";
    EXPECT_LE(priority, 1.0f) << "Priority should not exceed 1.0";
}

/**
 * @brief Test target selection with mixed scenarios
 */
TEST_F(ThreatBasedPrioritizerTest, SelectsHighestThreatTarget) {
    // Create diverse threat scenarios
    auto close_fast = TargetFactory::createApproachingTarget();   // High threat
    auto distant_slow = TargetFactory::createDistantTarget();     // Low threat
    auto high_conf = TargetFactory::createHighConfidenceTarget(); // Medium threat
    
    std::vector<Target*> targets = {&close_fast, &distant_slow, &high_conf};
    
    Target* selected = prioritizer->select_highest_priority_target(targets, *context);
    
    ASSERT_NE(selected, nullptr);
    
    // Verify selected target has highest priority
    float selected_priority = prioritizer->calculate_priority(*selected, *context);
    
    for (const auto* target : targets) {
        float target_priority = prioritizer->calculate_priority(*target, *context);
        EXPECT_LE(target_priority, selected_priority)
            << "Selected target should have highest or equal threat priority";
    }
}

/**
 * @brief Test custom threat parameters
 */
TEST_F(ThreatBasedPrioritizerTest, UsesCustomThreatParameters) {
    auto target = TargetFactory::createApproachingTarget();
    
    float default_priority = prioritizer->calculate_priority(target, *context);
    float custom_priority = custom_prioritizer->calculate_priority(target, *context);
    
    // Should get different results with different parameters
    // (unless by coincidence they're identical, which is unlikely)
    EXPECT_TRUE(std::abs(default_priority - custom_priority) > 0.001f)
        << "Custom parameters should produce different priorities";
}

/**
 * @brief Test zero velocity target
 */
TEST_F(ThreatBasedPrioritizerTest, HandlesZeroVelocityTarget) {
    Target stationary = TargetFactory::createHighConfidenceTarget();
    stationary.vx = stationary.vy = stationary.vz = 0.0f;
    
    // Should not crash or return NaN
    float priority = prioritizer->calculate_priority(stationary, *context);
    
    EXPECT_FALSE(std::isnan(priority)) << "Priority should not be NaN for stationary target";
    EXPECT_GE(priority, 0.0f);
    EXPECT_LE(priority, 1.0f);
}

/**
 * @brief Test target at origin
 */
TEST_F(ThreatBasedPrioritizerTest, HandlesTargetAtOrigin) {
    Target origin_target = TargetFactory::createHighConfidenceTarget();
    origin_target.x = origin_target.y = origin_target.z = 0.0f;
    origin_target.vx = origin_target.vy = origin_target.vz = 0.0f;
    
    // Should handle division by zero gracefully
    float priority = prioritizer->calculate_priority(origin_target, *context);
    
    EXPECT_FALSE(std::isnan(priority));
    EXPECT_FALSE(std::isinf(priority));
    EXPECT_GE(priority, 0.0f);
    EXPECT_LE(priority, 1.0f);
}

/**
 * @brief Test prioritizer name
 */
TEST_F(ThreatBasedPrioritizerTest, ReturnsCorrectName) {
    std::string name = prioritizer->get_name();
    EXPECT_EQ(name, "ThreatBasedPrioritizer");
}

/**
 * @brief Test that prioritization sorts correctly
 */
TEST_F(ThreatBasedPrioritizerTest, SortsTargetsByThreatLevel) {
    auto targets = TargetFactory::createTargetCluster();
    
    // Convert to pointer vector
    std::vector<Target*> target_pointers;
    for (auto& target : targets) {
        target_pointers.push_back(&target);
    }
    
    // Get priorities before sorting for comparison
    std::vector<float> original_priorities;
    for (const auto* target : target_pointers) {
        original_priorities.push_back(prioritizer->calculate_priority(*target, *context));
    }
    
    // Sort using prioritizer
    auto sorted_targets = prioritizer->prioritize_targets(target_pointers, *context);
    
    // Verify sorting (highest threat first)
    EXPECT_EQ(sorted_targets.size(), target_pointers.size());
    
    for (size_t i = 1; i < sorted_targets.size(); ++i) {
        float prev_priority = prioritizer->calculate_priority(*sorted_targets[i-1], *context);
        float curr_priority = prioritizer->calculate_priority(*sorted_targets[i], *context);
        
        EXPECT_GE(prev_priority, curr_priority)
            << "Targets not sorted by threat priority (descending)";
    }
}