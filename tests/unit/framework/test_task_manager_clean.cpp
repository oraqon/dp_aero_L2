#include <gtest/gtest.h>
#include "task_manager.h"
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
 * @brief Test Task status management
 */
TEST_F(TaskManagerTest, ManagesTaskStatus) {
    Task task("task_003", "target_003", Task::Type::POINT_GIMBAL);
    
    // Initial status should be CREATED
    EXPECT_EQ(task.get_status(), Task::Status::CREATED);
    
    // Update status
    EXPECT_NO_THROW(task.set_status(Task::Status::ASSIGNED));
    EXPECT_EQ(task.get_status(), Task::Status::ASSIGNED);
    
    EXPECT_NO_THROW(task.set_status(Task::Status::ACTIVE));
    EXPECT_EQ(task.get_status(), Task::Status::ACTIVE);
    
    EXPECT_NO_THROW(task.set_status(Task::Status::COMPLETED));
    EXPECT_EQ(task.get_status(), Task::Status::COMPLETED);
}

/**
 * @brief Test Task progress tracking
 */
TEST_F(TaskManagerTest, TracksTaskProgress) {
    Task task("task_004", "target_004", Task::Type::MONITOR_STATUS);
    
    // Initial progress should be 0
    EXPECT_FLOAT_EQ(task.get_progress(), 0.0f);
    
    // Set progress values
    task.set_progress(25.5f);
    EXPECT_FLOAT_EQ(task.get_progress(), 25.5f);
    
    task.set_progress(100.0f);
    EXPECT_FLOAT_EQ(task.get_progress(), 100.0f);
    
    // Test clamping - should not exceed 100%
    task.set_progress(150.0f);
    EXPECT_FLOAT_EQ(task.get_progress(), 100.0f);
    
    // Test clamping - should not go below 0%
    task.set_progress(-10.0f);
    EXPECT_FLOAT_EQ(task.get_progress(), 0.0f);
}

/**
 * @brief Test Task priority management
 */
TEST_F(TaskManagerTest, ManagesTaskPriority) {
    Task task("task_005", "target_005", Task::Type::CALIBRATE_SENSOR);
    
    // Initial priority should be NORMAL (default)
    EXPECT_EQ(task.get_priority(), Task::Priority::NORMAL);
    
    // Update priority
    task.set_priority(Task::Priority::CRITICAL);
    EXPECT_EQ(task.get_priority(), Task::Priority::CRITICAL);
    
    task.set_priority(Task::Priority::LOW);
    EXPECT_EQ(task.get_priority(), Task::Priority::LOW);
}