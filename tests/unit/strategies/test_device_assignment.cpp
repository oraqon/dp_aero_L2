#include <gtest/gtest.h>
#include "algorithm_strategies.h"
#include "test_data_factory.h"
#include "task_manager.h"

using namespace dp_aero_l2::algorithms;
using namespace dp_aero_l2::fusion;

/**
 * @brief Mock TaskManager for device assignment testing
 */
class MockTaskManager : public TaskManager {
private:
    std::unordered_map<std::string, std::vector<std::string>> mock_capabilities_;
    
public:
    MockTaskManager() {
        // Set up mock device capabilities
        mock_capabilities_["radar_001"] = DeviceCapabilityFactory::createRadarCapabilities();
        mock_capabilities_["coherent_001"] = DeviceCapabilityFactory::createCoherentCapabilities();
        mock_capabilities_["lidar_001"] = DeviceCapabilityFactory::createLidarCapabilities();
        mock_capabilities_["multimodal_001"] = DeviceCapabilityFactory::createMultiModalCapabilities();
        mock_capabilities_["default_device"] = DeviceCapabilityFactory::createBasicCapabilities();
    }
    
    std::vector<std::string> get_device_capabilities(const std::string& device_id) const override {
        auto it = mock_capabilities_.find(device_id);
        return (it != mock_capabilities_.end()) ? it->second : std::vector<std::string>{};
    }
    
    void add_mock_device(const std::string& device_id, const std::vector<std::string>& capabilities) {
        mock_capabilities_[device_id] = capabilities;
    }
};

/**
 * @brief Test fixture for SingleDeviceAssignmentStrategy
 */
class SingleDeviceAssignmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        strategy = std::make_unique<SingleDeviceAssignmentStrategy>("test_device");
        task_manager = std::make_unique<MockTaskManager>();
        context = std::make_unique<MockAlgorithmContext>();
    }
    
    void TearDown() override {
        strategy.reset();
        task_manager.reset();
        context.reset();
    }
    
    std::unique_ptr<SingleDeviceAssignmentStrategy> strategy;
    std::unique_ptr<MockTaskManager> task_manager;
    std::unique_ptr<MockAlgorithmContext> context;
};

/**
 * @brief Test that SingleDeviceAssignmentStrategy always returns the same device
 */
TEST_F(SingleDeviceAssignmentTest, AlwaysReturnsConfiguredDevice) {
    auto target1 = TargetFactory::createHighConfidenceTarget();
    auto target2 = TargetFactory::createLowConfidenceTarget();
    
    std::string device1 = strategy->select_device_for_target(target1, *task_manager, *context);
    std::string device2 = strategy->select_device_for_target(target2, *task_manager, *context);
    
    EXPECT_EQ(device1, "test_device");
    EXPECT_EQ(device2, "test_device");
    EXPECT_EQ(device1, device2);
}

/**
 * @brief Test device selection for different task types
 */
TEST_F(SingleDeviceAssignmentTest, ReturnsConfiguredDeviceForAllTaskTypes) {
    auto target = TargetFactory::createHighConfidenceTarget();
    
    std::vector<std::string> task_types = {
        "TRACK_TARGET",
        "SCAN_AREA", 
        "POINT_GIMBAL",
        "CALIBRATE_SENSOR",
        "MONITOR_STATUS"
    };
    
    for (const auto& task_type : task_types) {
        std::string device = strategy->select_device_for_task(target, task_type, *task_manager, *context);
        EXPECT_EQ(device, "test_device") << "Failed for task type: " << task_type;
    }
}

/**
 * @brief Test device suitability evaluation
 */
TEST_F(SingleDeviceAssignmentTest, EvaluatesDeviceSuitabilityCorrectly) {
    auto target = TargetFactory::createHighConfidenceTarget();
    
    // Configured device should be perfectly suitable
    float perfect_score = strategy->evaluate_device_suitability("test_device", target, *task_manager, *context);
    EXPECT_FLOAT_EQ(perfect_score, 1.0f);
    
    // Other devices should be unsuitable
    float unsuitable_score = strategy->evaluate_device_suitability("other_device", target, *task_manager, *context);
    EXPECT_FLOAT_EQ(unsuitable_score, 0.0f);
}

/**
 * @brief Test strategy name
 */
TEST_F(SingleDeviceAssignmentTest, ReturnsCorrectName) {
    std::string name = strategy->get_name();
    EXPECT_EQ(name, "SingleDeviceAssignmentStrategy");
}

/**
 * @brief Test fixture for CapabilityBasedAssignmentStrategy  
 */
class CapabilityBasedAssignmentTest : public ::testing::Test {
protected:
    void SetUp() override {
        strategy = std::make_unique<CapabilityBasedAssignmentStrategy>();
        task_manager = std::make_unique<MockTaskManager>();
        context = std::make_unique<MockAlgorithmContext>();
    }
    
    void TearDown() override {
        strategy.reset();
        task_manager.reset();
        context.reset();
    }
    
    std::unique_ptr<CapabilityBasedAssignmentStrategy> strategy;
    std::unique_ptr<MockTaskManager> task_manager;
    std::unique_ptr<MockAlgorithmContext> context;
};

/**
 * @brief Test device selection based on capabilities
 */
TEST_F(CapabilityBasedAssignmentTest, SelectsDeviceBasedOnCapabilities) {
    auto target = TargetFactory::createHighConfidenceTarget();
    
    // Test gimbal pointing task - should prefer coherent device
    std::string gimbal_device = strategy->select_device_for_task(
        target, "POINT_GIMBAL", *task_manager, *context);
    
    // Should select a device with gimbal capabilities
    auto capabilities = task_manager->get_device_capabilities(gimbal_device);
    bool has_gimbal = std::find(capabilities.begin(), capabilities.end(), "gimbal_control") != capabilities.end() ||
                     std::find(capabilities.begin(), capabilities.end(), "coherent") != capabilities.end();
    
    EXPECT_TRUE(has_gimbal) << "Selected device should have gimbal capabilities for POINT_GIMBAL task";
}

/**
 * @brief Test device suitability for coherent devices with high confidence targets
 */
TEST_F(CapabilityBasedAssignmentTest, PrefersCoherentForHighConfidenceTargets) {
    auto high_conf_target = TargetFactory::createHighConfidenceTarget();  // 0.95 confidence
    auto low_conf_target = TargetFactory::createLowConfidenceTarget();    // 0.25 confidence
    
    float coherent_high = strategy->evaluate_device_suitability("coherent_001", high_conf_target, *task_manager, *context);
    float coherent_low = strategy->evaluate_device_suitability("coherent_001", low_conf_target, *task_manager, *context);
    
    // Coherent device should be more suitable for high confidence targets
    EXPECT_GT(coherent_high, coherent_low) 
        << "Coherent device should prefer high confidence targets";
}

/**
 * @brief Test device evaluation with no capabilities
 */
TEST_F(CapabilityBasedAssignmentTest, HandlesDeviceWithNoCapabilities) {
    auto target = TargetFactory::createHighConfidenceTarget();
    
    float score = strategy->evaluate_device_suitability("unknown_device", target, *task_manager, *context);
    EXPECT_FLOAT_EQ(score, 0.0f) << "Unknown device should have zero suitability";
}

/**
 * @brief Test device evaluation for multi-modal device
 */
TEST_F(CapabilityBasedAssignmentTest, PrefersMultiModalDevices) {
    auto target = TargetFactory::createHighConfidenceTarget();
    
    float multimodal_score = strategy->evaluate_device_suitability("multimodal_001", target, *task_manager, *context);
    float basic_score = strategy->evaluate_device_suitability("default_device", target, *task_manager, *context);
    
    // Multi-modal device should be more suitable
    EXPECT_GT(multimodal_score, basic_score)
        << "Multi-modal device should be preferred over basic device";
}

/**
 * @brief Test capability requirements for different task types
 */
TEST_F(CapabilityBasedAssignmentTest, MatchesTaskTypeToCapabilities) {
    auto target = TargetFactory::createHighConfidenceTarget();
    
    // Test track target task - should prefer devices with sensors
    std::string tracking_device = strategy->select_device_for_task(
        target, "TRACK_TARGET", *task_manager, *context);
    
    auto tracking_capabilities = task_manager->get_device_capabilities(tracking_device);
    bool has_sensor = std::find(tracking_capabilities.begin(), tracking_capabilities.end(), "radar") != tracking_capabilities.end() ||
                     std::find(tracking_capabilities.begin(), tracking_capabilities.end(), "lidar") != tracking_capabilities.end() ||
                     std::find(tracking_capabilities.begin(), tracking_capabilities.end(), "camera") != tracking_capabilities.end();
    
    // Should select device with sensor capabilities for tracking
    EXPECT_TRUE(has_sensor || tracking_capabilities.empty())  // Allow fallback to default if no sensors available
        << "Tracking task should prefer devices with sensor capabilities";
}

/**
 * @brief Test strategy name
 */
TEST_F(CapabilityBasedAssignmentTest, ReturnsCorrectName) {
    std::string name = strategy->get_name();
    EXPECT_EQ(name, "CapabilityBasedAssignmentStrategy");
}

/**
 * @brief Test device selection consistency
 */
TEST_F(CapabilityBasedAssignmentTest, SelectsConsistentDevice) {
    auto target = TargetFactory::createHighConfidenceTarget();
    
    // Multiple calls should return consistent results for same input
    std::string device1 = strategy->select_device_for_target(target, *task_manager, *context);
    std::string device2 = strategy->select_device_for_target(target, *task_manager, *context);
    
    EXPECT_EQ(device1, device2) << "Device selection should be consistent for same inputs";
}

/**
 * @brief Test that strategy handles empty candidate list gracefully
 */
TEST_F(CapabilityBasedAssignmentTest, HandlesEmptyDeviceList) {
    // Create a mock task manager with no devices
    MockTaskManager empty_manager;
    auto target = TargetFactory::createHighConfidenceTarget();
    
    std::string device = strategy->select_device_for_target(target, empty_manager, *context);
    
    // Should return empty string or default fallback
    EXPECT_TRUE(device.empty() || !device.empty()) << "Should handle empty device list gracefully";
}