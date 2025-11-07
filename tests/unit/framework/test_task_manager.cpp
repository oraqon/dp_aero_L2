#include <gtest/gtest.h>
#include "task_manager.h"
#include "test_data_factory.h"
#include <chrono>
#include <thread>

using namespace dp_aero_l2::fusion;

/**
 * @brief Test fixture for TaskManager - Basic functionality tests
 */
class TaskManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        task_manager = std::make_unique<TaskManager>();
    }
    
    void TearDown() override {
        task_manager.reset();
    }
    
    std::unique_ptr<TaskManager> task_manager;
};

/**
 * @brief Test basic TaskManager instantiation
 */
TEST_F(TaskManagerTest, CanBeInstantiated) {
    EXPECT_NE(task_manager, nullptr);
}

/**
 * @brief Test device capability registration
 */
TEST_F(TaskManagerTest, RegistersDeviceCapabilities) {
    std::vector<std::string> capabilities = {"TRACKING", "SCANNING", "MONITORING"};
    
    // Register capabilities - this is a public method we can test
    EXPECT_NO_THROW(task_manager->register_device_capabilities("device_001", capabilities));
    
    // Register another device
    std::vector<std::string> other_caps = {"IMAGING", "RECORDING"};
    EXPECT_NO_THROW(task_manager->register_device_capabilities("device_002", other_caps));
}

/**
 * @brief Test Task creation with basic parameters
 */
TEST_F(TaskManagerTest, CreatesTaskWithValidParameters) {
    // Create a task using the actual constructor
    EXPECT_NO_THROW({
        Task task("task_001", "target_001", Task::Type::TRACK_TARGET, Task::Priority::HIGH);
        
        // Test basic getters
        EXPECT_EQ(task.get_task_id(), "task_001");
        EXPECT_EQ(task.get_target_id(), "target_001");
        EXPECT_EQ(task.get_type(), Task::Type::TRACK_TARGET);
        EXPECT_EQ(task.get_priority(), Task::Priority::HIGH);
        EXPECT_EQ(task.get_status(), Task::Status::CREATED);  // Should be initial status
    });
}

/**
 * @brief Test Task parameter management
 */
TEST_F(TaskManagerTest, ManagesTaskParameters) {
    Task task("task_002", "target_002", Task::Type::SCAN_AREA);
    
    // Set parameters
    EXPECT_NO_THROW({
        task.set_parameter("scan_radius", 100.0f);
        task.set_parameter("scan_frequency", 2);
        task.set_parameter("scan_mode", std::string("continuous"));
    });
    
    // Retrieve parameters
    auto radius = task.get_parameter<float>("scan_radius");
    auto frequency = task.get_parameter<int>("scan_frequency");
    auto mode = task.get_parameter<std::string>("scan_mode");
    
    ASSERT_TRUE(radius.has_value());
    ASSERT_TRUE(frequency.has_value());
    ASSERT_TRUE(mode.has_value());
    
    EXPECT_FLOAT_EQ(radius.value(), 100.0f);
    EXPECT_EQ(frequency.value(), 2);
    EXPECT_EQ(mode.value(), "continuous");
}

/**
 * @brief Test task status updates
 */
TEST_F(TaskManagerTest, UpdatesTaskStatusCorrectly) {
    auto task = createTestTask("task_001", "target_001", "device_001");
    task_manager->submit_task(task);
