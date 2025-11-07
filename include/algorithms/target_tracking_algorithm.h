#pragma once

#include "algorithm_framework.h"
#include <unordered_map>
#include <vector>
#include <cmath>

namespace dp_aero_l2::algorithms {

/**
 * @brief Example sensor fusion algorithm for target tracking
 * 
 * This algorithm demonstrates the framework by implementing a simple
 * multi-sensor target tracking system with state machine.
 * 
 * States:
 * - IDLE: No targets detected, waiting for sensor input
 * - ACQUIRING: Potential target detected, gathering more data
 * - TRACKING: Actively tracking confirmed target(s)
 * - LOST: Target lost, searching for reacquisition
 */
class TargetTrackingAlgorithm : public fusion::FusionAlgorithm {
private:
    struct Target {
        std::string target_id;
        float x, y, z;          // Position
        float vx, vy, vz;       // Velocity
        float confidence;       // Confidence score
        std::chrono::steady_clock::time_point last_update;
        std::unordered_map<std::string, int> sensor_detections; // Count per sensor
        
        // Default constructor (required for std::unordered_map)
        Target() : target_id(""), x(0), y(0), z(0), vx(0), vy(0), vz(0), confidence(0) {}
        
        // Parameterized constructor
        Target(const std::string& id) : target_id(id), x(0), y(0), z(0), 
                                       vx(0), vy(0), vz(0), confidence(0) {}
    };
    
    // Algorithm parameters
    struct Parameters {
        float min_confidence_threshold = 0.7f;
        float acquisition_threshold = 0.5f;
        float lost_threshold = 0.3f;
        int min_sensor_consensus = 2;  // Minimum sensors for confirmation
        std::chrono::seconds target_timeout{10};
        float position_noise = 0.1f;
        float velocity_alpha = 0.8f;   // Velocity smoothing factor
    };
    
    Parameters params_;
    std::chrono::steady_clock::time_point last_status_time_{};  // Instance-specific timing
    
public:
    std::string get_name() const override {
        return "TargetTrackingAlgorithm";
    }
    
    std::string get_version() const override {
        return "1.0.0";
    }
    
    std::string get_description() const override {
        return "Multi-sensor target tracking algorithm with state machine";
    }
    
    void initialize(fusion::AlgorithmContext& context) override {
        setup_state_machine();
        
        // Set initial state
        context.current_state_name = state_manager_.get_initial_state();
        context.current_state = state_manager_.get_state(context.current_state_name);
        
        // Initialize algorithm data
        context.set_data<std::unordered_map<std::string, Target>>("targets", {});
        context.set_data<int>("detection_count", 0);
        context.set_data<Parameters>("parameters", params_);
        
        // Register default device for first demo (single device operation)
        std::string default_device_id = "default_device";
        std::vector<std::string> capabilities = {"radar", "lidar", "camera", "gimbal_control"};
        get_task_manager().register_device_capabilities(default_device_id, capabilities);
        context.set_data<std::string>("default_device_id", default_device_id);
        
        // Enter initial state
        if (context.current_state && context.current_state->on_enter) {
            context.current_state->on_enter(context);
        }
        
        log_info("TargetTrackingAlgorithm initialized in state: " + context.current_state_name);
    }
    
    void process_l1_message(fusion::AlgorithmContext& context, 
                           const messages::L1ToL2Message& message) override {
        
        // Store message in history
        const std::string& node_id = message.sender().node_id();
        context.latest_l1_messages[node_id] = message;
        context.message_history[node_id].push_back(message);
        
        // Keep only recent messages (last 100)
        if (context.message_history[node_id].size() > 100) {
            context.message_history[node_id].erase(
                context.message_history[node_id].begin(),
                context.message_history[node_id].begin() + 50);
        }
        
        // Process sensor data
        if (message.has_sensor_data()) {
            process_sensor_data(context, node_id, message.sensor_data());
        }
        
        // Process capability advertisements
        if (message.has_capability()) {
            process_capability_advertisement(context, node_id, message.capability());
        }
    }
    
    void update(fusion::AlgorithmContext& context) override {
        // Update current state
        if (context.current_state && context.current_state->on_update) {
            context.current_state->on_update(context);
        }
        
        // Update all active tasks
        update_all_tasks(context);
        
        // Update target tracking
        update_target_tracking(context);
        
        // Check for state transitions
        check_state_transitions(context);
        
        // Send periodic updates
        send_status_updates(context);
    }
    
    void handle_trigger(fusion::AlgorithmContext& context, 
                       const std::string& trigger_name,
                       const std::any& trigger_data = {}) override {
        
        if (trigger_name == "reset") {
            log_info("Resetting algorithm");
            context.set_data<std::unordered_map<std::string, Target>>("targets", {});
            context.set_data<int>("detection_count", 0);
            trigger_transition(context, "reset");
            
        } else if (trigger_name == "node_timeout") {
            std::string node_id;
            try {
                node_id = std::any_cast<std::string>(trigger_data);
                log_warning("Node timeout: " + node_id);
                handle_node_timeout(context, node_id);
            } catch (const std::bad_any_cast&) {
                log_error("Invalid trigger data for node_timeout");
            }
            
        } else if (trigger_name == "target_detected") {
            trigger_transition(context, "detection");
            
        } else if (trigger_name == "target_lost") {
            trigger_transition(context, "lost");
            
        } else {
            // Try to trigger state transition
            trigger_transition(context, trigger_name);
        }
    }
    
    void shutdown(fusion::AlgorithmContext& context) override {
        // Send shutdown notification
        messages::L2ToL1Message shutdown_msg;
        shutdown_msg.set_message_id("shutdown_" + std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()));
        shutdown_msg.mutable_timestamp()->set_timestamp_ms(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        
        auto* sys_cmd = shutdown_msg.mutable_system_command();
        sys_cmd->set_command_type(messages::SystemCommand::SHUTDOWN);
        
        context.add_output_message(shutdown_msg);
        
        log_info("TargetTrackingAlgorithm shutdown");
    }

protected:
    void setup_state_machine() override {
        // Create states
        auto idle_state = std::make_shared<fusion::State>("IDLE");
        idle_state->on_enter = [this](fusion::AlgorithmContext& ctx) {
            log_info("Entered IDLE state");
            ctx.set_data<bool>("scanning", true);
        };
        idle_state->on_update = [this](fusion::AlgorithmContext& ctx) {
            // Look for potential targets
            scan_for_targets(ctx);
        };
        
        auto acquiring_state = std::make_shared<fusion::State>("ACQUIRING");
        acquiring_state->on_enter = [this](fusion::AlgorithmContext& ctx) {
            log_info("Entered ACQUIRING state");
            ctx.set_data<std::chrono::steady_clock::time_point>("acquisition_start", 
                std::chrono::steady_clock::now());
        };
        acquiring_state->on_update = [this](fusion::AlgorithmContext& ctx) {
            // Gather more data on potential targets
            evaluate_target_candidates(ctx);
        };
        
        auto tracking_state = std::make_shared<fusion::State>("TRACKING");
        tracking_state->on_enter = [this](fusion::AlgorithmContext& ctx) {
            log_info("Entered TRACKING state");
            send_gimbal_commands(ctx);
        };
        tracking_state->on_update = [this](fusion::AlgorithmContext& ctx) {
            // Update target positions and send tracking commands
            update_tracking(ctx);
        };
        
        auto lost_state = std::make_shared<fusion::State>("LOST");
        lost_state->on_enter = [this](fusion::AlgorithmContext& ctx) {
            log_info("Entered LOST state");
            ctx.set_data<std::chrono::steady_clock::time_point>("lost_start", 
                std::chrono::steady_clock::now());
        };
        lost_state->on_update = [this](fusion::AlgorithmContext& ctx) {
            // Search for lost targets
            search_for_lost_targets(ctx);
        };
        
        // Add states to state machine
        add_state("IDLE", idle_state);
        add_state("ACQUIRING", acquiring_state);
        add_state("TRACKING", tracking_state);
        add_state("LOST", lost_state);
        
        // Set initial state
        set_initial_state("IDLE");
        
        // Define transitions
        add_transition(fusion::Transition("IDLE", "ACQUIRING", "detection"));
        add_transition(fusion::Transition("ACQUIRING", "TRACKING", "confirmed"));
        add_transition(fusion::Transition("ACQUIRING", "IDLE", "false_positive"));
        add_transition(fusion::Transition("TRACKING", "LOST", "lost"));
        add_transition(fusion::Transition("LOST", "TRACKING", "reacquired"));
        add_transition(fusion::Transition("LOST", "IDLE", "timeout"));
        
        // Add reset transitions from any state
        add_transition(fusion::Transition("IDLE", "IDLE", "reset"));
        add_transition(fusion::Transition("ACQUIRING", "IDLE", "reset"));
        add_transition(fusion::Transition("TRACKING", "IDLE", "reset"));
        add_transition(fusion::Transition("LOST", "IDLE", "reset"));
    }

private:
    void process_sensor_data(fusion::AlgorithmContext& context, 
                           const std::string& node_id,
                           const data_streams::SensorData& sensor_data) {
        
        // Extract detection information based on sensor type
        if (sensor_data.has_radar()) {
            process_radar_detections(context, node_id, sensor_data.radar());
        } else if (sensor_data.has_lidar()) {
            process_lidar_data(context, node_id, sensor_data.lidar());
        } else if (sensor_data.has_image()) {
            process_image_data(context, node_id, sensor_data.image());
        }
    }
    
    void process_radar_detections(fusion::AlgorithmContext& context,
                                 const std::string& node_id,
                                 const data_streams::RadarData& radar_data) {
        
        auto targets_opt = context.get_data<std::unordered_map<std::string, Target>>("targets");
        if (!targets_opt) return;
        
        auto targets = *targets_opt;
        
        for (const auto& detection : radar_data.detections()) {
            if (detection.rcs() > 0.1f) {  // Filter small objects
                // Convert polar to cartesian
                float x = detection.range() * std::cos(detection.azimuth()) * std::cos(detection.elevation());
                float y = detection.range() * std::sin(detection.azimuth()) * std::cos(detection.elevation());
                float z = detection.range() * std::sin(detection.elevation());
                
                // Find or create target
                std::string target_id = find_closest_target(targets, x, y, z);
                if (target_id.empty()) {
                    target_id = "target_" + std::to_string(targets.size());
                    targets[target_id] = Target(target_id);
                    
                    // Create task for new target and assign to default device
                    auto default_device_id = context.get_data<std::string>("default_device_id");
                    if (default_device_id) {
                        std::string task_id = create_task_for_target(target_id, fusion::Task::Type::TRACK_TARGET, fusion::Task::Priority::HIGH);
                        assign_task_to_device(task_id, *default_device_id);
                        log_info("Created tracking task " + task_id + " for new target " + target_id);
                    }
                }
                
                // Update target
                update_target_position(targets[target_id], x, y, z, 0.8f, node_id);
            }
        }
        
        context.set_data("targets", targets);
        
        if (!targets.empty()) {
            handle_trigger(context, "target_detected");
        }
    }
    
    void process_lidar_data(fusion::AlgorithmContext& context,
                           const std::string& node_id,
                           const data_streams::LidarData& lidar_data) {
        // Simple clustering algorithm for object detection
        // This is a simplified example - real implementation would be more sophisticated
        
        auto targets_opt = context.get_data<std::unordered_map<std::string, Target>>("targets");
        if (!targets_opt) return;
        
        auto targets = *targets_opt;
        
        // Basic clustering - group points that are close together
        std::vector<std::vector<data_streams::LidarData::Point>> clusters;
        cluster_lidar_points(lidar_data.points(), clusters, 1.0f); // 1m cluster distance
        
        for (const auto& cluster : clusters) {
            if (cluster.size() > 10) {  // Minimum points for object
                // Calculate cluster centroid
                float x = 0, y = 0, z = 0;
                for (const auto& point : cluster) {
                    x += point.x();
                    y += point.y();
                    z += point.z();
                }
                x /= cluster.size();
                y /= cluster.size();
                z /= cluster.size();
                
                // Find or create target
                std::string target_id = find_closest_target(targets, x, y, z);
                if (target_id.empty()) {
                    target_id = "target_" + std::to_string(targets.size());
                    targets[target_id] = Target(target_id);
                    
                    // Create task for new target and assign to default device
                    auto default_device_id = context.get_data<std::string>("default_device_id");
                    if (default_device_id) {
                        std::string task_id = create_task_for_target(target_id, fusion::Task::Type::TRACK_TARGET, fusion::Task::Priority::HIGH);
                        assign_task_to_device(task_id, *default_device_id);
                        log_info("Created tracking task " + task_id + " for new target " + target_id);
                    }
                }
                
                update_target_position(targets[target_id], x, y, z, 0.6f, node_id);
            }
        }
        
        context.set_data("targets", targets);
    }
    
    void process_image_data(fusion::AlgorithmContext& context,
                           const std::string& node_id,
                           const data_streams::ImageData& image_data) {
        // Placeholder for computer vision processing
        // In real implementation, this would run object detection algorithms
        
        log_debug("Processing image data from " + node_id + 
                 " (" + std::to_string(image_data.width()) + "x" + 
                 std::to_string(image_data.height()) + ")");
    }
    
    void process_capability_advertisement(fusion::AlgorithmContext& context,
                                        const std::string& node_id,
                                        const messages::CapabilityAdvertisement& capability) {
        log_info("Node " + node_id + " advertised capabilities: " + 
                std::to_string(capability.sensor_types_size()) + " sensor types");
    }
    
    void scan_for_targets(fusion::AlgorithmContext& context) {
        // In IDLE state, actively scan for potential targets
        auto detection_count = context.get_data<int>("detection_count").value_or(0);
        
        if (detection_count > 0) {
            handle_trigger(context, "target_detected");
        }
    }
    
    void evaluate_target_candidates(fusion::AlgorithmContext& context) {
        auto targets_opt = context.get_data<std::unordered_map<std::string, Target>>("targets");
        if (!targets_opt) return;
        
        auto targets = *targets_opt;
        auto params = context.get_data<Parameters>("parameters").value_or(params_);
        
        bool confirmed_target = false;
        for (auto& [id, target] : targets) {
            if (target.confidence > params.acquisition_threshold && 
                target.sensor_detections.size() >= params.min_sensor_consensus) {
                target.confidence = std::min(1.0f, target.confidence + 0.1f);
                
                if (target.confidence > params.min_confidence_threshold) {
                    confirmed_target = true;
                }
            }
        }
        
        context.set_data("targets", targets);
        
        if (confirmed_target) {
            handle_trigger(context, "confirmed");
        }
    }
    
    void update_tracking(fusion::AlgorithmContext& context) {
        auto targets_opt = context.get_data<std::unordered_map<std::string, Target>>("targets");
        if (!targets_opt) return;
        
        auto targets = *targets_opt;
        auto params = context.get_data<Parameters>("parameters").value_or(params_);
        
        bool has_valid_targets = false;
        auto now = std::chrono::steady_clock::now();
        
        for (auto& [id, target] : targets) {
            // Check if target is still valid
            if (now - target.last_update > params.target_timeout) {
                target.confidence *= 0.9f;  // Decay confidence
            }
            
            if (target.confidence > params.lost_threshold) {
                has_valid_targets = true;
                send_gimbal_command_for_target(context, target);
            }
        }
        
        context.set_data("targets", targets);
        
        if (!has_valid_targets) {
            handle_trigger(context, "lost");
        }
    }
    
    void search_for_lost_targets(fusion::AlgorithmContext& context) {
        // Implement search pattern for lost targets
        auto lost_start = context.get_data<std::chrono::steady_clock::time_point>("lost_start");
        if (lost_start) {
            auto now = std::chrono::steady_clock::now();
            if (now - *lost_start > std::chrono::seconds(30)) {
                handle_trigger(context, "timeout");
            }
        }
    }
    
    void update_target_tracking(fusion::AlgorithmContext& context) {
        // Prediction and update cycle for all targets
        auto targets_opt = context.get_data<std::unordered_map<std::string, Target>>("targets");
        if (!targets_opt) return;
        
        auto targets = *targets_opt;
        auto params = context.get_data<Parameters>("parameters").value_or(params_);
        
        // Remove old targets
        auto now = std::chrono::steady_clock::now();
        for (auto it = targets.begin(); it != targets.end();) {
            if (now - it->second.last_update > params.target_timeout * 2) {
                log_info("Removing old target: " + it->first);
                it = targets.erase(it);
            } else {
                ++it;
            }
        }
        
        context.set_data("targets", targets);
    }
    
    void check_state_transitions(fusion::AlgorithmContext& context) {
        auto targets_opt = context.get_data<std::unordered_map<std::string, Target>>("targets");
        if (!targets_opt) return;
        
        auto targets = *targets_opt;
        int detection_count = 0;
        
        for (const auto& [id, target] : targets) {
            if (target.confidence > 0.3f) {
                detection_count++;
            }
        }
        
        context.set_data("detection_count", detection_count);
    }
    
    void send_status_updates(fusion::AlgorithmContext& context) {
        // Send periodic status updates to L1 nodes
        auto now = std::chrono::steady_clock::now();
        
        if (last_status_time_ == std::chrono::steady_clock::time_point{} || 
            now - last_status_time_ > std::chrono::seconds(5)) {
            auto targets_opt = context.get_data<std::unordered_map<std::string, Target>>("targets");
            if (targets_opt) {
                send_fusion_results(context, *targets_opt);
            }
            last_status_time_ = now;
        }
    }
    
    void send_gimbal_commands(fusion::AlgorithmContext& context) {
        auto targets_opt = context.get_data<std::unordered_map<std::string, Target>>("targets");
        if (!targets_opt) return;
        
        auto targets = *targets_opt;
        
        // Find highest confidence target
        Target* best_target = nullptr;
        for (auto& [id, target] : targets) {
            if (!best_target || target.confidence > best_target->confidence) {
                best_target = &target;
            }
        }
        
        if (best_target) {
            send_gimbal_command_for_target(context, *best_target);
        }
    }
    
    void send_gimbal_command_for_target(fusion::AlgorithmContext& context, const Target& target) {
        messages::L2ToL1Message gimbal_cmd;
        gimbal_cmd.set_message_id("gimbal_" + std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()));
        gimbal_cmd.mutable_timestamp()->set_timestamp_ms(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        
        // Target coherent device specifically
        gimbal_cmd.set_target_node_id("coherent_001");
        
        auto* control_cmd = gimbal_cmd.mutable_control_command();
        control_cmd->set_command_type(messages::ControlCommand::POINT_GIMBAL);
        
        // Calculate gimbal angles
        float range = std::sqrt(target.x * target.x + target.y * target.y + target.z * target.z);
        float theta = std::atan2(target.y, target.x);  // Azimuth
        float phi = std::asin(target.z / range);       // Elevation
        
        control_cmd->mutable_target_position()->set_theta(theta);
        control_cmd->mutable_target_position()->set_phi(phi);
        
        context.add_output_message(gimbal_cmd);
        
        log_info("*** TASKING COHERENT DEVICE ***");
        log_info("Sent gimbal command to coherent_001 for target " + target.target_id + 
                 " (theta: " + std::to_string(theta) + ", phi: " + std::to_string(phi) + ")");
    }
    
    void send_fusion_results(fusion::AlgorithmContext& context, 
                            const std::unordered_map<std::string, Target>& targets) {
        messages::L2ToL1Message result_msg;
        result_msg.set_message_id("fusion_result_" + std::to_string(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()));
        result_msg.mutable_timestamp()->set_timestamp_ms(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        
        auto* fusion_result = result_msg.mutable_fusion_result();
        fusion_result->set_algorithm_name(get_name());
        fusion_result->set_result_type("target_tracks");
        fusion_result->set_confidence(calculate_overall_confidence(targets));
        
        // Serialize target data (simplified)
        std::string target_data = "Targets: " + std::to_string(targets.size()) + 
                                 ", State: " + context.current_state_name;
        fusion_result->set_result_data(target_data);
        
        context.add_output_message(result_msg);
    }
    
    void handle_node_timeout(fusion::AlgorithmContext& context, const std::string& node_id) {
        // Handle node timeout - might affect target confidence
        auto targets_opt = context.get_data<std::unordered_map<std::string, Target>>("targets");
        if (!targets_opt) return;
        
        auto targets = *targets_opt;
        
        for (auto& [id, target] : targets) {
            if (target.sensor_detections.count(node_id) > 0) {
                target.confidence *= 0.8f;  // Reduce confidence for targets detected by timed-out node
                target.sensor_detections.erase(node_id);
            }
        }
        
        context.set_data("targets", targets);
    }
    
    // Helper functions
    std::string find_closest_target(const std::unordered_map<std::string, Target>& targets,
                                   float x, float y, float z, float max_distance = 5.0f) {
        std::string closest_id;
        float min_distance = max_distance;
        
        for (const auto& [id, target] : targets) {
            float dx = target.x - x;
            float dy = target.y - y;
            float dz = target.z - z;
            float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
            
            if (distance < min_distance) {
                min_distance = distance;
                closest_id = id;
            }
        }
        
        return closest_id;
    }
    
    void update_target_position(Target& target, float x, float y, float z, 
                               float confidence_boost, const std::string& sensor_id) {
        auto now = std::chrono::steady_clock::now();
        
        // Simple position filtering
        float alpha = params_.position_noise;
        target.x = target.x * (1 - alpha) + x * alpha;
        target.y = target.y * (1 - alpha) + y * alpha;
        target.z = target.z * (1 - alpha) + z * alpha;
        
        // Update velocity
        if (target.last_update != std::chrono::steady_clock::time_point{}) {
            auto dt = std::chrono::duration<float>(now - target.last_update).count();
            if (dt > 0) {
                float new_vx = (x - target.x) / dt;
                float new_vy = (y - target.y) / dt;
                float new_vz = (z - target.z) / dt;
                
                target.vx = target.vx * params_.velocity_alpha + new_vx * (1 - params_.velocity_alpha);
                target.vy = target.vy * params_.velocity_alpha + new_vy * (1 - params_.velocity_alpha);
                target.vz = target.vz * params_.velocity_alpha + new_vz * (1 - params_.velocity_alpha);
            }
        }
        
        target.confidence = std::min(1.0f, target.confidence + confidence_boost);
        target.last_update = now;
        target.sensor_detections[sensor_id]++;
    }
    
    void cluster_lidar_points(const google::protobuf::RepeatedPtrField<data_streams::LidarData::Point>& points,
                             std::vector<std::vector<data_streams::LidarData::Point>>& clusters,
                             float cluster_distance) {
        // Simple clustering algorithm
        std::vector<bool> visited(points.size(), false);
        
        for (int i = 0; i < points.size(); ++i) {
            if (visited[i]) continue;
            
            std::vector<data_streams::LidarData::Point> cluster;
            std::queue<int> to_visit;
            to_visit.push(i);
            visited[i] = true;
            
            while (!to_visit.empty()) {
                int current = to_visit.front();
                to_visit.pop();
                cluster.push_back(points[current]);
                
                for (int j = 0; j < points.size(); ++j) {
                    if (visited[j]) continue;
                    
                    float dx = points[current].x() - points[j].x();
                    float dy = points[current].y() - points[j].y();
                    float dz = points[current].z() - points[j].z();
                    float distance = std::sqrt(dx*dx + dy*dy + dz*dz);
                    
                    if (distance < cluster_distance) {
                        visited[j] = true;
                        to_visit.push(j);
                    }
                }
            }
            
            if (cluster.size() > 5) {  // Minimum cluster size
                clusters.push_back(cluster);
            }
        }
    }
    
    float calculate_overall_confidence(const std::unordered_map<std::string, Target>& targets) {
        if (targets.empty()) return 0.0f;
        
        float total_confidence = 0.0f;
        for (const auto& [id, target] : targets) {
            total_confidence += target.confidence;
        }
        
        return total_confidence / targets.size();
    }
    
    void log_info(const std::string& message) {
        std::cout << "[" << get_name() << "] INFO: " << message << std::endl;
    }
    
    void log_debug(const std::string& message) {
        std::cout << "[" << get_name() << "] DEBUG: " << message << std::endl;
    }
    
    void log_warning(const std::string& message) {
        std::cout << "[" << get_name() << "] WARNING: " << message << std::endl;
    }
    
    void log_error(const std::string& message) {
        std::cerr << "[" << get_name() << "] ERROR: " << message << std::endl;
    }
};

} // namespace dp_aero_l2::algorithms