#include <gtest/gtest.h>
#include "algorithm_framework.h"
#include "test_data_factory.h"
#include "../fixtures/mock_algorithm_context.h"

using namespace dp_aero_l2::fusion;

/**
 * @brief Mock implementation of FusionAlgorithm for testing
 */
class MockFusionAlgorithm : public FusionAlgorithm {
public:
    MockFusionAlgorithm(const std::string& name) : algorithm_name_(name) {
        initialize_call_count_ = 0;
        process_call_count_ = 0;
        finalize_call_count_ = 0;
        last_processed_message_id_ = "";
        should_fail_processing_ = false;
    }
    
    void initialize(AlgorithmContext& context) override {
        initialize_call_count_++;
        context.set_data("algorithm_initialized", true);
        context.set_data("algorithm_name", algorithm_name_);
        
        if (should_fail_initialization_) {
            throw std::runtime_error("Initialization failed");
        }
    }
    
    void process_message(const messages::L1ToL2Message& message, AlgorithmContext& context) override {
        process_call_count_++;
        last_processed_message_id_ = message.message_id();
        
        context.set_data("last_processed_message", message.message_id());
        context.set_data("process_call_count", process_call_count_);
        
        if (should_fail_processing_) {
            throw std::runtime_error("Processing failed");
        }
        
        // Generate output message
        messages::L2ToL1Message output;
        output.set_message_id("response_to_" + message.message_id());
        output.set_target_node_id(message.source_node_id());
        context.add_output_message(output);
    }
    
    void finalize(AlgorithmContext& context) override {
        finalize_call_count_++;
        context.set_data("algorithm_finalized", true);
        
        if (should_fail_finalization_) {
            throw std::runtime_error("Finalization failed");
        }
    }
    
    std::string get_algorithm_name() const override {
        return algorithm_name_;
    }
    
    // Test control methods
    void set_should_fail_initialization(bool should_fail) { 
        should_fail_initialization_ = should_fail; 
    }
    
    void set_should_fail_processing(bool should_fail) { 
        should_fail_processing_ = should_fail; 
    }
    
    void set_should_fail_finalization(bool should_fail) { 
        should_fail_finalization_ = should_fail; 
    }
    
    // Test inspection methods
    int get_initialize_call_count() const { return initialize_call_count_; }
    int get_process_call_count() const { return process_call_count_; }
    int get_finalize_call_count() const { return finalize_call_count_; }
    std::string get_last_processed_message_id() const { return last_processed_message_id_; }
    
private:
    std::string algorithm_name_;
    int initialize_call_count_;
    int process_call_count_;
    int finalize_call_count_;
    std::string last_processed_message_id_;
    bool should_fail_initialization_ = false;
    bool should_fail_processing_ = false;
    bool should_fail_finalization_ = false;
};

/**
 * @brief Test fixture for FusionAlgorithm base class
 */
class FusionAlgorithmTest : public ::testing::Test {
protected:
    void SetUp() override {
        context = std::make_unique<AlgorithmContext>();
        algorithm = std::make_unique<MockFusionAlgorithm>("TestAlgorithm");
    }
    
    void TearDown() override {
        algorithm.reset();
        context.reset();
    }
    
    std::unique_ptr<AlgorithmContext> context;
    std::unique_ptr<MockFusionAlgorithm> algorithm;
    
    // Helper to create test messages
    messages::L1ToL2Message createTestMessage(const std::string& msg_id, 
                                            const std::string& source_node) {
        messages::L1ToL2Message message;
        message.set_message_id(msg_id);
        message.set_source_node_id(source_node);
        message.mutable_timestamp()->set_seconds(
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        return message;
    }
};

/**
 * @brief Test algorithm initialization
 */
TEST_F(FusionAlgorithmTest, InitializesCorrectly) {
    // Algorithm should not be initialized initially
    auto init_flag = context->get_data<bool>("algorithm_initialized");
    EXPECT_FALSE(init_flag.has_value());
    
    // Initialize algorithm
    EXPECT_NO_THROW(algorithm->initialize(*context));
    
    // Verify initialization occurred
    EXPECT_EQ(algorithm->get_initialize_call_count(), 1);
    
    // Verify context was updated
    init_flag = context->get_data<bool>("algorithm_initialized");
    ASSERT_TRUE(init_flag.has_value());
    EXPECT_TRUE(init_flag.value());
    
    auto name = context->get_data<std::string>("algorithm_name");
    ASSERT_TRUE(name.has_value());
    EXPECT_EQ(name.value(), "TestAlgorithm");
}

/**
 * @brief Test algorithm initialization failure handling
 */
TEST_F(FusionAlgorithmTest, HandlesInitializationFailure) {
    algorithm->set_should_fail_initialization(true);
    
    EXPECT_THROW(algorithm->initialize(*context), std::runtime_error);
    
    // Verify call count still incremented
    EXPECT_EQ(algorithm->get_initialize_call_count(), 1);
}

/**
 * @brief Test message processing
 */
TEST_F(FusionAlgorithmTest, ProcessesMessagesCorrectly) {
    // Initialize first
    algorithm->initialize(*context);
    
    // Create and process message
    auto message = createTestMessage("msg_001", "radar_node");
    
    EXPECT_NO_THROW(algorithm->process_message(message, *context));
    
    // Verify processing occurred
    EXPECT_EQ(algorithm->get_process_call_count(), 1);
    EXPECT_EQ(algorithm->get_last_processed_message_id(), "msg_001");
    
    // Verify context updates
    auto last_msg = context->get_data<std::string>("last_processed_message");
    ASSERT_TRUE(last_msg.has_value());
    EXPECT_EQ(last_msg.value(), "msg_001");
    
    auto count = context->get_data<int>("process_call_count");
    ASSERT_TRUE(count.has_value());
    EXPECT_EQ(count.value(), 1);
    
    // Verify output message was generated
    EXPECT_EQ(context->pending_outputs.size(), 1);
    EXPECT_EQ(context->pending_outputs[0].message_id(), "response_to_msg_001");
    EXPECT_EQ(context->pending_outputs[0].target_node_id(), "radar_node");
}

/**
 * @brief Test processing multiple messages
 */
TEST_F(FusionAlgorithmTest, ProcessesMultipleMessages) {
    algorithm->initialize(*context);
    
    // Process multiple messages
    auto msg1 = createTestMessage("msg_001", "radar_node");
    auto msg2 = createTestMessage("msg_002", "camera_node");
    auto msg3 = createTestMessage("msg_003", "lidar_node");
    
    algorithm->process_message(msg1, *context);
    algorithm->process_message(msg2, *context);
    algorithm->process_message(msg3, *context);
    
    // Verify processing counts
    EXPECT_EQ(algorithm->get_process_call_count(), 3);
    EXPECT_EQ(algorithm->get_last_processed_message_id(), "msg_003");
    
    // Verify output messages
    EXPECT_EQ(context->pending_outputs.size(), 3);
    EXPECT_EQ(context->pending_outputs[0].message_id(), "response_to_msg_001");
    EXPECT_EQ(context->pending_outputs[1].message_id(), "response_to_msg_002");
    EXPECT_EQ(context->pending_outputs[2].message_id(), "response_to_msg_003");
}

/**
 * @brief Test message processing failure handling
 */
TEST_F(FusionAlgorithmTest, HandlesProcessingFailure) {
    algorithm->initialize(*context);
    algorithm->set_should_fail_processing(true);
    
    auto message = createTestMessage("msg_001", "radar_node");
    
    EXPECT_THROW(algorithm->process_message(message, *context), std::runtime_error);
    
    // Verify call count still incremented
    EXPECT_EQ(algorithm->get_process_call_count(), 1);
}

/**
 * @brief Test algorithm finalization
 */
TEST_F(FusionAlgorithmTest, FinalizesCorrectly) {
    algorithm->initialize(*context);
    
    // Process some messages
    auto message = createTestMessage("msg_001", "radar_node");
    algorithm->process_message(message, *context);
    
    // Finalize
    EXPECT_NO_THROW(algorithm->finalize(*context));
    
    // Verify finalization occurred
    EXPECT_EQ(algorithm->get_finalize_call_count(), 1);
    
    // Verify context was updated
    auto final_flag = context->get_data<bool>("algorithm_finalized");
    ASSERT_TRUE(final_flag.has_value());
    EXPECT_TRUE(final_flag.value());
}

/**
 * @brief Test finalization failure handling
 */
TEST_F(FusionAlgorithmTest, HandlesFinalizationFailure) {
    algorithm->initialize(*context);
    algorithm->set_should_fail_finalization(true);
    
    EXPECT_THROW(algorithm->finalize(*context), std::runtime_error);
    
    // Verify call count still incremented
    EXPECT_EQ(algorithm->get_finalize_call_count(), 1);
}

/**
 * @brief Test algorithm name retrieval
 */
TEST_F(FusionAlgorithmTest, ReturnsCorrectAlgorithmName) {
    EXPECT_EQ(algorithm->get_algorithm_name(), "TestAlgorithm");
    
    // Create another algorithm with different name
    auto other_algorithm = std::make_unique<MockFusionAlgorithm>("OtherAlgorithm");
    EXPECT_EQ(other_algorithm->get_algorithm_name(), "OtherAlgorithm");
}

/**
 * @brief Test algorithm lifecycle
 */
TEST_F(FusionAlgorithmTest, ExecutesFullLifecycle) {
    // Full lifecycle: initialize -> process -> finalize
    
    // 1. Initialize
    algorithm->initialize(*context);
    EXPECT_EQ(algorithm->get_initialize_call_count(), 1);
    
    // 2. Process multiple messages
    for (int i = 0; i < 5; ++i) {
        auto msg = createTestMessage("msg_" + std::to_string(i), "node_" + std::to_string(i));
        algorithm->process_message(msg, *context);
    }
    EXPECT_EQ(algorithm->get_process_call_count(), 5);
    
    // 3. Finalize
    algorithm->finalize(*context);
    EXPECT_EQ(algorithm->get_finalize_call_count(), 1);
    
    // Verify final state
    auto init_flag = context->get_data<bool>("algorithm_initialized");
    auto final_flag = context->get_data<bool>("algorithm_finalized");
    
    ASSERT_TRUE(init_flag.has_value() && final_flag.has_value());
    EXPECT_TRUE(init_flag.value() && final_flag.value());
    
    // Verify all output messages were generated
    EXPECT_EQ(context->pending_outputs.size(), 5);
}

/**
 * @brief Test algorithm without initialization
 */
TEST_F(FusionAlgorithmTest, ProcessesWithoutExplicitInitialization) {
    // Some algorithms might work without explicit initialization
    auto message = createTestMessage("msg_001", "radar_node");
    
    // Should still process the message successfully
    EXPECT_NO_THROW(algorithm->process_message(message, *context));
    
    EXPECT_EQ(algorithm->get_process_call_count(), 1);
    EXPECT_EQ(algorithm->get_initialize_call_count(), 0);  // Not initialized
}

/**
 * @brief Test context persistence across operations
 */
TEST_F(FusionAlgorithmTest, MaintainsContextPersistence) {
    // Initialize with some context data
    context->set_data("external_config", std::string("test_value"));
    
    algorithm->initialize(*context);
    
    // Verify external data persists
    auto config = context->get_data<std::string>("external_config");
    ASSERT_TRUE(config.has_value());
    EXPECT_EQ(config.value(), "test_value");
    
    // Process message
    auto message = createTestMessage("msg_001", "radar_node");
    algorithm->process_message(message, *context);
    
    // Verify both external and algorithm data persist
    config = context->get_data<std::string>("external_config");
    auto algo_name = context->get_data<std::string>("algorithm_name");
    auto last_msg = context->get_data<std::string>("last_processed_message");
    
    ASSERT_TRUE(config.has_value());
    ASSERT_TRUE(algo_name.has_value());
    ASSERT_TRUE(last_msg.has_value());
    
    EXPECT_EQ(config.value(), "test_value");
    EXPECT_EQ(algo_name.value(), "TestAlgorithm");
    EXPECT_EQ(last_msg.value(), "msg_001");
    
    // Finalize
    algorithm->finalize(*context);
    
    // All data should still be accessible
    config = context->get_data<std::string>("external_config");
    auto final_flag = context->get_data<bool>("algorithm_finalized");
    
    ASSERT_TRUE(config.has_value() && final_flag.has_value());
    EXPECT_EQ(config.value(), "test_value");
    EXPECT_TRUE(final_flag.value());
}