#include <gtest/gtest.h>
#include "algorithm_strategies.h"
#include "test_data_factory.h"
#include <algorithm>
#include <memory>

using namespace dp_aero_l2::algorithms;
using namespace dp_aero_l2::fusion;

/**
 * @brief Test fixture for ConfidenceBasedPrioritizer
 */
class ConfidenceBasedPrioritizerTest : public ::testing::Test {
protected:
    void SetUp() override {
        prioritizer = std::make_unique<ConfidenceBasedPrioritizer>();
        context = std::make_unique<MockAlgorithmContext>();
    }
    
    void TearDown() override {
        prioritizer.reset();
        context.reset();
    }
    
    std::unique_ptr<ConfidenceBasedPrioritizer> prioritizer;
    std::unique_ptr<MockAlgorithmContext> context;
};

/**
 * @brief Test basic priority calculation
 */
TEST_F(ConfidenceBasedPrioritizerTest, CalculatesPriorityCorrectly) {
    // Create targets with known confidence values
    auto high_conf_target = TargetFactory::createHighConfidenceTarget();
    auto low_conf_target = TargetFactory::createLowConfidenceTarget();
    
    // Calculate priorities
    float high_priority = prioritizer->calculate_priority(high_conf_target, *context);
    float low_priority = prioritizer->calculate_priority(low_conf_target, *context);
    
    // Verify priority equals confidence for confidence-based prioritizer
    EXPECT_FLOAT_EQ(high_priority, high_conf_target.confidence);
    EXPECT_FLOAT_EQ(low_priority, low_conf_target.confidence);
    
    // Verify higher confidence = higher priority
    EXPECT_GT(high_priority, low_priority);
}

/**
 * @brief Test target prioritization (sorting)
 */
TEST_F(ConfidenceBasedPrioritizerTest, PrioritizesTargetsCorrectly) {
    // Create targets with varying confidence levels
    auto targets = TargetFactory::createSortableTargets();
    
    // Convert to pointer vector (as required by interface)
    std::vector<Target*> target_pointers;
    for (auto& target : targets) {
        target_pointers.push_back(&target);
    }
    
    // Shuffle to ensure we're actually sorting
    std::random_shuffle(target_pointers.begin(), target_pointers.end());
    
    // Prioritize targets
    auto sorted_targets = prioritizer->prioritize_targets(target_pointers, *context);
    
    // Verify sorting (highest confidence first)
    EXPECT_EQ(sorted_targets.size(), target_pointers.size());
    
    for (size_t i = 1; i < sorted_targets.size(); ++i) {
        EXPECT_GE(sorted_targets[i-1]->confidence, sorted_targets[i]->confidence) 
            << "Targets not sorted by confidence (descending)";
    }
}

/**
 * @brief Test selecting highest priority target
 */
TEST_F(ConfidenceBasedPrioritizerTest, SelectsHighestPriorityTarget) {
    // Create mixed target cluster
    auto targets = TargetFactory::createTargetCluster();
    
    // Convert to pointer vector
    std::vector<Target*> target_pointers;
    for (auto& target : targets) {
        target_pointers.push_back(&target);
    }
    
    // Select highest priority target
    Target* best_target = prioritizer->select_highest_priority_target(target_pointers, *context);
    
    // Verify we got a valid result
    ASSERT_NE(best_target, nullptr);
    
    // Verify it's actually the highest confidence target
    float best_confidence = best_target->confidence;
    for (const auto* target : target_pointers) {
        EXPECT_LE(target->confidence, best_confidence) 
            << "Selected target does not have highest confidence";
    }
}

/**
 * @brief Test with empty target list
 */
TEST_F(ConfidenceBasedPrioritizerTest, HandlesEmptyTargetList) {
    std::vector<Target*> empty_targets;
    
    // Should return nullptr for empty list
    Target* result = prioritizer->select_highest_priority_target(empty_targets, *context);
    EXPECT_EQ(result, nullptr);
    
    // Prioritize should return empty list
    auto sorted = prioritizer->prioritize_targets(empty_targets, *context);
    EXPECT_TRUE(sorted.empty());
}

/**
 * @brief Test with single target
 */
TEST_F(ConfidenceBasedPrioritizerTest, HandlesSingleTarget) {
    auto target = TargetFactory::createHighConfidenceTarget();
    std::vector<Target*> single_target = {&target};
    
    // Select should return the only target
    Target* result = prioritizer->select_highest_priority_target(single_target, *context);
    EXPECT_EQ(result, &target);
    
    // Priority should equal confidence
    float priority = prioritizer->calculate_priority(target, *context);
    EXPECT_FLOAT_EQ(priority, target.confidence);
}

/**
 * @brief Test with identical confidence targets
 */
TEST_F(ConfidenceBasedPrioritizerTest, HandlesIdenticalConfidenceTargets) {
    // Create targets with same confidence
    auto target1 = TargetFactory::createHighConfidenceTarget();
    auto target2 = TargetFactory::createHighConfidenceTarget();
    target2.target_id = "identical_conf_002";
    
    std::vector<Target*> identical_targets = {&target1, &target2};
    
    // Should select one of them (stable result)
    Target* result = prioritizer->select_highest_priority_target(identical_targets, *context);
    EXPECT_NE(result, nullptr);
    EXPECT_TRUE(result == &target1 || result == &target2);
    
    // Both should have same priority
    float priority1 = prioritizer->calculate_priority(target1, *context);
    float priority2 = prioritizer->calculate_priority(target2, *context);
    EXPECT_FLOAT_EQ(priority1, priority2);
}

/**
 * @brief Test prioritizer name
 */
TEST_F(ConfidenceBasedPrioritizerTest, ReturnsCorrectName) {
    std::string name = prioritizer->get_name();
    EXPECT_EQ(name, "ConfidenceBasedPrioritizer");
}

/**
 * @brief Test with extreme confidence values
 */
TEST_F(ConfidenceBasedPrioritizerTest, HandlesExtremeConfidenceValues) {
    Target min_conf_target = TargetFactory::createLowConfidenceTarget();
    Target max_conf_target = TargetFactory::createHighConfidenceTarget();
    
    min_conf_target.confidence = 0.0f;
    max_conf_target.confidence = 1.0f;
    
    std::vector<Target*> extreme_targets = {&min_conf_target, &max_conf_target};
    
    // Should select max confidence target
    Target* result = prioritizer->select_highest_priority_target(extreme_targets, *context);
    EXPECT_EQ(result, &max_conf_target);
    
    // Priorities should match confidence values
    EXPECT_FLOAT_EQ(prioritizer->calculate_priority(min_conf_target, *context), 0.0f);
    EXPECT_FLOAT_EQ(prioritizer->calculate_priority(max_conf_target, *context), 1.0f);
}