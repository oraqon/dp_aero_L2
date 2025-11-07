#include <gtest/gtest.h>
#include "algorithm_framework.h"
#include "test_data_factory.h"
#include <string>
#include <vector>
#include <chrono>

using namespace dp_aero_l2::fusion;

/**
 * @brief Test fixture for AlgorithmContext
 */
class AlgorithmContextTest : public ::testing::Test {
protected:
    void SetUp() override {
        context = std::make_unique<AlgorithmContext>();
    }
    
    void TearDown() override {
        context.reset();
    }
    
    std::unique_ptr<AlgorithmContext> context;
};

/**
 * @brief Test basic data storage and retrieval
 */
TEST_F(AlgorithmContextTest, StoresAndRetrievesDataCorrectly) {
    // Test int storage
    context->set_data("test_int", 42);
    auto retrieved_int = context->get_data<int>("test_int");
    
    ASSERT_TRUE(retrieved_int.has_value());
    EXPECT_EQ(retrieved_int.value(), 42);
    
    // Test string storage
    std::string test_string = "hello_world";
    context->set_data("test_string", test_string);
    auto retrieved_string = context->get_data<std::string>("test_string");
    
    ASSERT_TRUE(retrieved_string.has_value());
    EXPECT_EQ(retrieved_string.value(), test_string);
    
    // Test float storage
    context->set_data("test_float", 3.14159f);
    auto retrieved_float = context->get_data<float>("test_float");
    
    ASSERT_TRUE(retrieved_float.has_value());
    EXPECT_FLOAT_EQ(retrieved_float.value(), 3.14159f);
}

/**
 * @brief Test complex data type storage
 */
TEST_F(AlgorithmContextTest, StoresComplexDataTypes) {
    // Test vector storage
    std::vector<int> test_vector = {1, 2, 3, 4, 5};
    context->set_data("test_vector", test_vector);
    auto retrieved_vector = context->get_data<std::vector<int>>("test_vector");
    
    ASSERT_TRUE(retrieved_vector.has_value());
    EXPECT_EQ(retrieved_vector.value(), test_vector);
    
    // Test custom struct storage (using Target from our framework)
    auto test_target = TargetFactory::createHighConfidenceTarget();
    context->set_data("test_target", test_target);
    auto retrieved_target = context->get_data<Target>("test_target");
    
    ASSERT_TRUE(retrieved_target.has_value());
    EXPECT_EQ(retrieved_target.value().target_id, test_target.target_id);
    EXPECT_FLOAT_EQ(retrieved_target.value().confidence, test_target.confidence);
}

/**
 * @brief Test data overwriting
 */
TEST_F(AlgorithmContextTest, OverwritesDataCorrectly) {
    // Store initial value
    context->set_data("counter", 10);
    auto initial = context->get_data<int>("counter");
    ASSERT_TRUE(initial.has_value());
    EXPECT_EQ(initial.value(), 10);
    
    // Overwrite with new value
    context->set_data("counter", 20);
    auto updated = context->get_data<int>("counter");
    ASSERT_TRUE(updated.has_value());
    EXPECT_EQ(updated.value(), 20);
}

/**
 * @brief Test retrieval of non-existent data
 */
TEST_F(AlgorithmContextTest, HandlesNonExistentData) {
    auto non_existent = context->get_data<int>("non_existent_key");
    EXPECT_FALSE(non_existent.has_value());
}

/**
 * @brief Test type safety - wrong type retrieval
 */
TEST_F(AlgorithmContextTest, HandlesWrongTypeRetrieval) {
    // Store as int
    context->set_data("test_data", 42);
    
    // Try to retrieve as string - should fail
    auto wrong_type = context->get_data<std::string>("test_data");
    EXPECT_FALSE(wrong_type.has_value());
    
    // Correct type should still work
    auto correct_type = context->get_data<int>("test_data");
    ASSERT_TRUE(correct_type.has_value());
    EXPECT_EQ(correct_type.value(), 42);
}

/**
 * @brief Test state management
 */
TEST_F(AlgorithmContextTest, ManagesStateCorrectly) {
    // Initial state should be empty
    EXPECT_TRUE(context->current_state_name.empty());
    
    // Set state
    context->current_state_name = "INITIALIZING";
    EXPECT_EQ(context->current_state_name, "INITIALIZING");
    
    // Change state
    context->current_state_name = "RUNNING";
    EXPECT_EQ(context->current_state_name, "RUNNING");
}

/**
 * @brief Test data persistence across operations
 */
TEST_F(AlgorithmContextTest, MaintainsDataPersistence) {
    // Store multiple pieces of data
    context->set_data("config_value", std::string("test_config"));
    context->set_data("iteration_count", 0);
    context->set_data("start_time", std::chrono::steady_clock::now());
    
    // Perform some operations
    context->current_state_name = "PROCESSING";
    
    // Verify all data is still accessible
    auto config = context->get_data<std::string>("config_value");
    auto count = context->get_data<int>("iteration_count");
    auto start = context->get_data<std::chrono::steady_clock::time_point>("start_time");
    
    ASSERT_TRUE(config.has_value());
    ASSERT_TRUE(count.has_value());
    ASSERT_TRUE(start.has_value());
    
    EXPECT_EQ(config.value(), "test_config");
    EXPECT_EQ(count.value(), 0);
    EXPECT_EQ(context->current_state_name, "PROCESSING");
}

/**
 * @brief Test large data storage
 */
TEST_F(AlgorithmContextTest, HandlesLargeDataSets) {
    // Store large vector
    std::vector<Target> large_target_list;
    for (int i = 0; i < 1000; ++i) {
        auto target = TargetFactory::createHighConfidenceTarget();
        target.target_id = "target_" + std::to_string(i);
        large_target_list.push_back(target);
    }
    
    context->set_data("large_target_list", large_target_list);
    
    auto retrieved = context->get_data<std::vector<Target>>("large_target_list");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved.value().size(), 1000);
    EXPECT_EQ(retrieved.value()[0].target_id, "target_0");
    EXPECT_EQ(retrieved.value()[999].target_id, "target_999");
}

/**
 * @brief Test concurrent-like access patterns (simulation)
 */
TEST_F(AlgorithmContextTest, HandlesFrequentAccess) {
    // Simulate frequent read/write operations
    const int num_operations = 100;
    
    for (int i = 0; i < num_operations; ++i) {
        std::string key = "key_" + std::to_string(i % 10);  // Reuse keys
        context->set_data(key, i);
        
        auto value = context->get_data<int>(key);
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(value.value(), i);
    }
    
    // Verify final state
    for (int i = 0; i < 10; ++i) {
        std::string key = "key_" + std::to_string(i);
        auto value = context->get_data<int>(key);
        ASSERT_TRUE(value.has_value());
        // Should have the last written value for this key
        int expected = 90 + i;  // Last iteration for each key
        EXPECT_EQ(value.value(), expected);
    }
}