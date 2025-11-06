#include "redis_utils.h"
#include "l1_to_l2.pb.h"
#include "l2_to_l1.pb.h"
#include <iostream>
#include <thread>
#include <random>
#include <signal.h>
#include <iomanip>

using namespace dp_aero_l2;

std::atomic<bool> running{true};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down L1 node...\n";
    running = false;
}

class L1NodeSimulator {
private:
    std::string node_id_;
    std::string node_type_;
    std::string location_;
    std::unique_ptr<redis_utils::RedisMessenger> redis_messenger_;
    std::thread publisher_thread_;
    std::thread subscriber_thread_;
    
    // Simulation parameters
    std::mt19937 rng_;
    std::uniform_real_distribution<float> position_dist_{-100.0f, 100.0f};
    std::uniform_real_distribution<float> angle_dist_{-M_PI, M_PI};
    std::uniform_real_distribution<float> range_dist_{10.0f, 200.0f};
    std::uniform_real_distribution<float> confidence_dist_{0.3f, 0.9f};
    std::uniform_int_distribution<int> detection_count_dist_{1, 5};
    
    // Configuration
    std::chrono::milliseconds publish_interval_{1000};
    float detection_probability_ = 0.3f;  // Probability of generating detections
    
public:
    L1NodeSimulator(const std::string& node_id, const std::string& node_type, 
                   const std::string& location, const std::string& redis_url = "tcp://127.0.0.1:6379")
        : node_id_(node_id), node_type_(node_type), location_(location), rng_(std::random_device{}()) {
        
        redis_messenger_ = std::make_unique<redis_utils::RedisMessenger>(redis_url);
    }
    
    void start() {
        std::cout << "Starting L1 Node Simulator: " << node_id_ << " (" << node_type_ << ")\n";
        
        // Send initial capability advertisement
        send_capability_advertisement();
        
        // Start publisher thread
        publisher_thread_ = std::thread(&L1NodeSimulator::publisher_loop, this);
        
        // Start subscriber thread to listen for L2 commands
        subscriber_thread_ = std::thread(&L1NodeSimulator::subscriber_loop, this);
        
        std::cout << "L1 Node " << node_id_ << " started successfully\n";
    }
    
    void stop() {
        running = false;
        
        if (publisher_thread_.joinable()) {
            publisher_thread_.join();
        }
        
        if (subscriber_thread_.joinable()) {
            subscriber_thread_.join();
        }
        
        std::cout << "L1 Node " << node_id_ << " stopped\n";
    }
    
    void set_publish_interval(std::chrono::milliseconds interval) {
        publish_interval_ = interval;
    }
    
    void set_detection_probability(float probability) {
        detection_probability_ = std::clamp(probability, 0.0f, 1.0f);
    }

private:
    void publisher_loop() {
        int message_count = 0;
        
        while (running) {
            try {
                // Send heartbeat every few messages
                if (message_count % 10 == 0) {
                    send_heartbeat();
                }
                
                // Send node status occasionally
                if (message_count % 20 == 0) {
                    send_node_status();
                }
                
                // Generate and send sensor data
                if (std::uniform_real_distribution<float>(0.0f, 1.0f)(rng_) < detection_probability_) {
                    send_sensor_data();
                }
                
                message_count++;
                
            } catch (const std::exception& e) {
                std::cerr << "[" << node_id_ << "] Publisher error: " << e.what() << std::endl;
            }
            
            std::this_thread::sleep_for(publish_interval_);
        }
    }
    
    void subscriber_loop() {
        try {
            redis_messenger_->subscribe<messages::L2ToL1Message>("l2_to_l1",
                [this](const messages::L2ToL1Message& message) {
                    handle_l2_message(message);
                });
        } catch (const std::exception& e) {
            std::cerr << "[" << node_id_ << "] Subscriber error: " << e.what() << std::endl;
        }
    }
    
    void send_capability_advertisement() {
        messages::L1ToL2Message msg;
        msg.set_message_id(generate_message_id());
        msg.set_sequence_number(0);
        
        // Set sender information
        auto* sender = msg.mutable_sender();
        sender->set_node_id(node_id_);
        sender->set_node_type(node_type_);
        sender->set_location(location_);
        (*sender->mutable_metadata())["simulator"] = "true";
        (*sender->mutable_metadata())["version"] = "1.0.0";
        
        // Set timestamp
        auto* timestamp = msg.mutable_timestamp();
        timestamp->set_timestamp_ms(get_current_timestamp_ms());
        
        // Set capability information
        auto* capability = msg.mutable_capability();
        capability->set_node_id(node_id_);
        capability->add_sensor_types(node_type_);
        capability->set_update_rate_hz(1000.0f / publish_interval_.count());
        
        if (node_type_ == "radar") {
            capability->add_data_formats("radar_detections");
            (*capability->mutable_parameters())["max_range"] = "200.0";
            (*capability->mutable_parameters())["angular_resolution"] = "0.1";
        } else if (node_type_ == "lidar") {
            capability->add_data_formats("point_cloud");
            (*capability->mutable_parameters())["max_range"] = "150.0";
            (*capability->mutable_parameters())["num_points"] = "65536";
        } else if (node_type_ == "camera") {
            capability->add_data_formats("rgb_image");
            (*capability->mutable_parameters())["resolution"] = "1920x1080";
            (*capability->mutable_parameters())["fps"] = "30";
        }
        
        redis_messenger_->publish("l1_to_l2", msg);
        std::cout << "[" << node_id_ << "] Sent capability advertisement\n";
    }
    
    void send_heartbeat() {
        messages::L1ToL2Message msg;
        msg.set_message_id(generate_message_id());
        
        auto* sender = msg.mutable_sender();
        sender->set_node_id(node_id_);
        sender->set_node_type(node_type_);
        sender->set_location(location_);
        
        auto* timestamp = msg.mutable_timestamp();
        timestamp->set_timestamp_ms(get_current_timestamp_ms());
        
        auto* heartbeat = msg.mutable_heartbeat();
        heartbeat->mutable_timestamp()->set_timestamp_ms(get_current_timestamp_ms());
        heartbeat->set_node_id(node_id_);
        (*heartbeat->mutable_status_info())["status"] = "operational";
        (*heartbeat->mutable_status_info())["cpu_usage"] = std::to_string(
            std::uniform_real_distribution<float>(10.0f, 50.0f)(rng_));
        
        redis_messenger_->publish("l1_to_l2", msg);
        
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::cout << "[" << node_id_ << "] " 
                 << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
                 << " Heartbeat sent\n";
    }
    
    void send_node_status() {
        messages::L1ToL2Message msg;
        msg.set_message_id(generate_message_id());
        
        auto* sender = msg.mutable_sender();
        sender->set_node_id(node_id_);
        sender->set_node_type(node_type_);
        sender->set_location(location_);
        
        auto* timestamp = msg.mutable_timestamp();
        timestamp->set_timestamp_ms(get_current_timestamp_ms());
        
        auto* status = msg.mutable_node_status();
        status->set_node_id(node_id_);
        status->set_status(common::NodeStatus::ONLINE);
        status->mutable_last_heartbeat()->set_timestamp_ms(get_current_timestamp_ms());
        status->set_cpu_usage(std::uniform_real_distribution<float>(10.0f, 60.0f)(rng_));
        status->set_memory_usage(std::uniform_real_distribution<float>(20.0f, 80.0f)(rng_));
        
        redis_messenger_->publish("l1_to_l2", msg);
    }
    
    void send_sensor_data() {
        messages::L1ToL2Message msg;
        msg.set_message_id(generate_message_id());
        
        auto* sender = msg.mutable_sender();
        sender->set_node_id(node_id_);
        sender->set_node_type(node_type_);
        sender->set_location(location_);
        
        auto* timestamp = msg.mutable_timestamp();
        timestamp->set_timestamp_ms(get_current_timestamp_ms());
        
        auto* sensor_data = msg.mutable_sensor_data();
        
        if (node_type_ == "radar") {
            generate_radar_data(sensor_data->mutable_radar());
        } else if (node_type_ == "lidar") {
            generate_lidar_data(sensor_data->mutable_lidar());
        } else if (node_type_ == "camera") {
            generate_image_data(sensor_data->mutable_image());
        } else if (node_type_ == "imu") {
            generate_imu_data(sensor_data->mutable_imu());
        } else if (node_type_ == "gps") {
            generate_gps_data(sensor_data->mutable_gps());
        }
        
        redis_messenger_->publish("l1_to_l2", msg);
        
        std::cout << "[" << node_id_ << "] Sent " << node_type_ << " data\n";
    }
    
    void generate_radar_data(data_streams::RadarData* radar_data) {
        radar_data->set_max_range(200.0f);
        radar_data->set_angular_resolution(0.1f);
        
        int num_detections = detection_count_dist_(rng_);
        for (int i = 0; i < num_detections; ++i) {
            auto* detection = radar_data->add_detections();
            detection->set_range(range_dist_(rng_));
            detection->set_azimuth(angle_dist_(rng_));
            detection->set_elevation(std::uniform_real_distribution<float>(-M_PI/4, M_PI/4)(rng_));
            detection->set_velocity(std::uniform_real_distribution<float>(-50.0f, 50.0f)(rng_));
            detection->set_rcs(std::uniform_real_distribution<float>(0.1f, 10.0f)(rng_));
        }
    }
    
    void generate_lidar_data(data_streams::LidarData* lidar_data) {
        lidar_data->set_angular_resolution(0.05f);
        lidar_data->set_range_min(0.5f);
        lidar_data->set_range_max(150.0f);
        
        // Generate clustered points to simulate objects
        int num_clusters = std::uniform_int_distribution<int>(1, 3)(rng_);
        
        for (int cluster = 0; cluster < num_clusters; ++cluster) {
            // Cluster center
            float center_x = position_dist_(rng_);
            float center_y = position_dist_(rng_);
            float center_z = std::uniform_real_distribution<float>(-5.0f, 5.0f)(rng_);
            
            int points_in_cluster = std::uniform_int_distribution<int>(20, 100)(rng_);
            
            for (int i = 0; i < points_in_cluster; ++i) {
                auto* point = lidar_data->add_points();
                
                // Add noise around cluster center
                point->set_x(center_x + std::normal_distribution<float>(0.0f, 1.0f)(rng_));
                point->set_y(center_y + std::normal_distribution<float>(0.0f, 1.0f)(rng_));
                point->set_z(center_z + std::normal_distribution<float>(0.0f, 0.5f)(rng_));
                point->set_intensity(std::uniform_real_distribution<float>(0.1f, 1.0f)(rng_));
            }
        }
        
        lidar_data->set_num_points(lidar_data->points_size());
    }
    
    void generate_image_data(data_streams::ImageData* image_data) {
        image_data->set_width(1920);
        image_data->set_height(1080);
        image_data->set_channels(3);
        image_data->set_encoding("rgb8");
        image_data->set_exposure_time(std::uniform_real_distribution<float>(1.0f, 100.0f)(rng_));
        image_data->set_gain(std::uniform_real_distribution<float>(1.0f, 4.0f)(rng_));
        
        // Generate dummy image data (small for simulation)
        std::string dummy_data(1024, 0);  // 1KB dummy data
        std::iota(dummy_data.begin(), dummy_data.end(), 0);
        image_data->set_image_data(dummy_data);
    }
    
    void generate_imu_data(data_streams::ImuData* imu_data) {
        auto* accel = imu_data->mutable_linear_acceleration();
        accel->set_x(std::normal_distribution<float>(0.0f, 0.1f)(rng_));
        accel->set_y(std::normal_distribution<float>(0.0f, 0.1f)(rng_));
        accel->set_z(std::normal_distribution<float>(9.81f, 0.1f)(rng_));
        
        auto* gyro = imu_data->mutable_angular_velocity();
        gyro->set_x(std::normal_distribution<float>(0.0f, 0.01f)(rng_));
        gyro->set_y(std::normal_distribution<float>(0.0f, 0.01f)(rng_));
        gyro->set_z(std::normal_distribution<float>(0.0f, 0.01f)(rng_));
        
        auto* mag = imu_data->mutable_magnetic_field();
        mag->set_x(std::normal_distribution<float>(0.2f, 0.05f)(rng_));
        mag->set_y(std::normal_distribution<float>(0.0f, 0.05f)(rng_));
        mag->set_z(std::normal_distribution<float>(0.4f, 0.05f)(rng_));
    }
    
    void generate_gps_data(data_streams::GpsData* gps_data) {
        // Simulate GPS coordinates around a fixed location
        static double base_lat = 37.7749;  // San Francisco
        static double base_lon = -122.4194;
        
        gps_data->set_latitude(base_lat + std::normal_distribution<double>(0.0, 0.001)(rng_));
        gps_data->set_longitude(base_lon + std::normal_distribution<double>(0.0, 0.001)(rng_));
        gps_data->set_altitude(std::uniform_real_distribution<float>(50.0f, 150.0f)(rng_));
        gps_data->set_speed(std::uniform_real_distribution<float>(0.0f, 20.0f)(rng_));
        gps_data->set_heading(std::uniform_real_distribution<float>(0.0f, 360.0f)(rng_));
        gps_data->set_num_satellites(std::uniform_int_distribution<int>(6, 12)(rng_));
        gps_data->set_hdop(std::uniform_real_distribution<float>(0.8f, 2.0f)(rng_));
    }
    
    void handle_l2_message(const messages::L2ToL1Message& message) {
        // Check if message is for this node or broadcast
        if (!message.target_node_id().empty() && message.target_node_id() != node_id_) {
            return;  // Message not for this node
        }
        
        std::cout << "[" << node_id_ << "] Received L2 message: ";
        
        switch (message.payload_case()) {
            case messages::L2ToL1Message::kControlCommand:
                handle_control_command(message.control_command());
                break;
            case messages::L2ToL1Message::kConfigUpdate:
                handle_config_update(message.config_update());
                break;
            case messages::L2ToL1Message::kFusionResult:
                handle_fusion_result(message.fusion_result());
                break;
            case messages::L2ToL1Message::kSystemCommand:
                handle_system_command(message.system_command());
                break;
            default:
                std::cout << "Unknown message type\n";
                break;
        }
    }
    
    void handle_control_command(const messages::ControlCommand& command) {
        std::cout << "Control Command - ";
        
        switch (command.command_type()) {
            case messages::ControlCommand::START_SENSOR:
                std::cout << "START_SENSOR\n";
                break;
            case messages::ControlCommand::STOP_SENSOR:
                std::cout << "STOP_SENSOR\n";
                break;
            case messages::ControlCommand::CHANGE_RATE:
                std::cout << "CHANGE_RATE to " << command.target_rate_hz() << " Hz\n";
                if (command.target_rate_hz() > 0) {
                    publish_interval_ = std::chrono::milliseconds(
                        static_cast<int>(1000.0f / command.target_rate_hz()));
                }
                break;
            case messages::ControlCommand::POINT_GIMBAL:
                std::cout << "POINT_GIMBAL - theta: " << command.target_position().theta() 
                         << ", phi: " << command.target_position().phi() << "\n";
                break;
            case messages::ControlCommand::CALIBRATE:
                std::cout << "CALIBRATE\n";
                break;
            case messages::ControlCommand::RESET:
                std::cout << "RESET\n";
                break;
            default:
                std::cout << "Unknown command\n";
                break;
        }
    }
    
    void handle_config_update(const messages::ConfigurationUpdate& config) {
        std::cout << "Configuration Update - Section: " << config.config_section() << "\n";
        
        for (const auto& [key, value] : config.config_parameters()) {
            std::cout << "  " << key << " = " << value << "\n";
            
            // Handle some common configuration parameters
            if (key == "detection_probability") {
                try {
                    detection_probability_ = std::stof(value);
                    detection_probability_ = std::clamp(detection_probability_, 0.0f, 1.0f);
                } catch (...) {
                    std::cerr << "Invalid detection_probability value: " << value << "\n";
                }
            }
        }
    }
    
    void handle_fusion_result(const messages::FusionResult& result) {
        std::cout << "Fusion Result - Algorithm: " << result.algorithm_name() 
                 << ", Type: " << result.result_type() 
                 << ", Confidence: " << result.confidence() << "\n";
    }
    
    void handle_system_command(const messages::SystemCommand& command) {
        std::cout << "System Command - ";
        
        switch (command.command_type()) {
            case messages::SystemCommand::SHUTDOWN:
                std::cout << "SHUTDOWN\n";
                running = false;
                break;
            case messages::SystemCommand::RESTART:
                std::cout << "RESTART\n";
                break;
            case messages::SystemCommand::SYNC_TIME:
                std::cout << "SYNC_TIME\n";
                break;
            default:
                std::cout << "Unknown system command\n";
                break;
        }
    }
    
    std::string generate_message_id() {
        static std::atomic<uint64_t> counter{0};
        return node_id_ + "_" + std::to_string(counter++);
    }
    
    int64_t get_current_timestamp_ms() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --node-id <id>             Node identifier (required)\n";
    std::cout << "  --node-type <type>         Node type: radar, lidar, camera, imu, gps (required)\n";
    std::cout << "  --location <location>      Node location description (required)\n";
    std::cout << "  --redis-url <url>          Redis connection URL (default: tcp://127.0.0.1:6379)\n";
    std::cout << "  --interval <ms>            Publish interval in milliseconds (default: 1000)\n";
    std::cout << "  --detection-prob <prob>    Detection probability 0.0-1.0 (default: 0.3)\n";
    std::cout << "  --help                     Show this help message\n";
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::string node_id, node_type, location;
    std::string redis_url = "tcp://127.0.0.1:6379";
    int interval_ms = 1000;
    float detection_prob = 0.3f;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            print_usage(argv[0]);
            return 0;
        } else if (arg == "--node-id" && i + 1 < argc) {
            node_id = argv[++i];
        } else if (arg == "--node-type" && i + 1 < argc) {
            node_type = argv[++i];
        } else if (arg == "--location" && i + 1 < argc) {
            location = argv[++i];
        } else if (arg == "--redis-url" && i + 1 < argc) {
            redis_url = argv[++i];
        } else if (arg == "--interval" && i + 1 < argc) {
            interval_ms = std::stoi(argv[++i]);
        } else if (arg == "--detection-prob" && i + 1 < argc) {
            detection_prob = std::stof(argv[++i]);
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Validate required arguments
    if (node_id.empty() || node_type.empty() || location.empty()) {
        std::cerr << "Error: --node-id, --node-type, and --location are required\n";
        print_usage(argv[0]);
        return 1;
    }
    
    // Validate node type
    std::vector<std::string> valid_types = {"radar", "lidar", "camera", "imu", "gps"};
    if (std::find(valid_types.begin(), valid_types.end(), node_type) == valid_types.end()) {
        std::cerr << "Error: Invalid node type. Valid types: radar, lidar, camera, imu, gps\n";
        return 1;
    }
    
    try {
        // Create and start L1 node simulator
        L1NodeSimulator simulator(node_id, node_type, location, redis_url);
        simulator.set_publish_interval(std::chrono::milliseconds(interval_ms));
        simulator.set_detection_probability(detection_prob);
        
        simulator.start();
        
        std::cout << "\nL1 Node Simulator running. Press Ctrl+C to stop.\n";
        std::cout << "Configuration:\n";
        std::cout << "  Node ID: " << node_id << "\n";
        std::cout << "  Type: " << node_type << "\n";
        std::cout << "  Location: " << location << "\n";
        std::cout << "  Publish Interval: " << interval_ms << " ms\n";
        std::cout << "  Detection Probability: " << detection_prob << "\n\n";
        
        // Keep running until signal
        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        simulator.stop();
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "L1 Node Simulator stopped.\n";
    return 0;
}