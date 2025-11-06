#pragma once

#include "algorithm_framework.h"
#include "redis_utils.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <unordered_set>

namespace dp_aero_l2::core {

/**
 * @brief Configuration for L2 fusion system
 */
struct L2Config {
    std::string redis_connection = "tcp://127.0.0.1:6379";
    std::string l1_to_l2_topic = "l1_to_l2";
    std::string l2_to_l1_topic = "l2_to_l1";
    std::string heartbeat_topic = "l2_heartbeat";
    
    // Node management
    std::chrono::seconds node_timeout{30};
    std::chrono::seconds heartbeat_interval{5};
    
    // Algorithm configuration
    std::string algorithm_name = "default";
    std::chrono::milliseconds algorithm_update_interval{100};
    
    // Threading
    size_t worker_threads = 2;
    size_t message_queue_size = 1000;
    
    // Logging
    bool enable_debug_logging = false;
    std::string log_level = "INFO";
};

/**
 * @brief Node registry for tracking L1 nodes
 */
class NodeRegistry {
private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, common::NodeIdentity> nodes_;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_seen_;
    std::unordered_map<std::string, common::NodeStatus> node_status_;
    
public:
    void register_node(const common::NodeIdentity& node) {
        std::unique_lock lock(mutex_);
        nodes_[node.node_id()] = node;
        last_seen_[node.node_id()] = std::chrono::steady_clock::now();
    }
    
    void update_node_heartbeat(const std::string& node_id) {
        std::unique_lock lock(mutex_);
        last_seen_[node_id] = std::chrono::steady_clock::now();
    }
    
    void update_node_status(const std::string& node_id, const common::NodeStatus& status) {
        std::unique_lock lock(mutex_);
        node_status_[node_id] = status;
        last_seen_[node_id] = std::chrono::steady_clock::now();
    }
    
    std::vector<std::string> get_active_nodes(std::chrono::seconds timeout) const {
        std::shared_lock lock(mutex_);
        std::vector<std::string> active_nodes;
        auto now = std::chrono::steady_clock::now();
        
        for (const auto& [node_id, last_seen] : last_seen_) {
            if (now - last_seen < timeout) {
                active_nodes.push_back(node_id);
            }
        }
        return active_nodes;
    }
    
    std::vector<std::string> get_timed_out_nodes(std::chrono::seconds timeout) const {
        std::shared_lock lock(mutex_);
        std::vector<std::string> timed_out_nodes;
        auto now = std::chrono::steady_clock::now();
        
        for (const auto& [node_id, last_seen] : last_seen_) {
            if (now - last_seen >= timeout) {
                timed_out_nodes.push_back(node_id);
            }
        }
        return timed_out_nodes;
    }
    
    std::optional<common::NodeIdentity> get_node(const std::string& node_id) const {
        std::shared_lock lock(mutex_);
        auto it = nodes_.find(node_id);
        return (it != nodes_.end()) ? std::make_optional(it->second) : std::nullopt;
    }
    
    std::vector<common::NodeIdentity> get_all_nodes() const {
        std::shared_lock lock(mutex_);
        std::vector<common::NodeIdentity> all_nodes;
        for (const auto& [id, node] : nodes_) {
            all_nodes.push_back(node);
        }
        return all_nodes;
    }
    
    void remove_node(const std::string& node_id) {
        std::unique_lock lock(mutex_);
        nodes_.erase(node_id);
        last_seen_.erase(node_id);
        node_status_.erase(node_id);
    }
};

/**
 * @brief Main L2 fusion system manager
 */
class L2FusionManager {
private:
    L2Config config_;
    std::unique_ptr<redis_utils::RedisMessenger> redis_messenger_;
    std::unique_ptr<fusion::FusionAlgorithm> algorithm_;
    fusion::AlgorithmContext algorithm_context_;
    NodeRegistry node_registry_;
    
    // Threading
    std::atomic<bool> running_{false};
    std::vector<std::thread> worker_threads_;
    std::thread algorithm_thread_;
    std::thread heartbeat_thread_;
    std::thread node_monitor_thread_;
    
    // Message queue
    std::queue<messages::L1ToL2Message> message_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    
    // Statistics
    std::atomic<uint64_t> messages_processed_{0};
    std::atomic<uint64_t> messages_sent_{0};
    std::chrono::steady_clock::time_point start_time_;
    
public:
    explicit L2FusionManager(const L2Config& config = L2Config{})
        : config_(config), start_time_(std::chrono::steady_clock::now()) {
        redis_messenger_ = std::make_unique<redis_utils::RedisMessenger>(config_.redis_connection);
    }
    
    ~L2FusionManager() {
        stop();
    }
    
    /**
     * @brief Set the fusion algorithm to use
     */
    void set_algorithm(std::unique_ptr<fusion::FusionAlgorithm> algorithm) {
        if (running_) {
            throw std::runtime_error("Cannot change algorithm while system is running");
        }
        algorithm_ = std::move(algorithm);
    }
    
    /**
     * @brief Start the L2 fusion system
     */
    void start() {
        if (running_) {
            return;
        }
        
        if (!algorithm_) {
            throw std::runtime_error("No algorithm set. Call set_algorithm() before start()");
        }
        
        running_ = true;
        
        // Initialize algorithm
        algorithm_->initialize(algorithm_context_);
        
        // Start worker threads
        for (size_t i = 0; i < config_.worker_threads; ++i) {
            worker_threads_.emplace_back(&L2FusionManager::worker_thread_func, this);
        }
        
        // Start algorithm thread
        algorithm_thread_ = std::thread(&L2FusionManager::algorithm_thread_func, this);
        
        // Start heartbeat thread
        heartbeat_thread_ = std::thread(&L2FusionManager::heartbeat_thread_func, this);
        
        // Start node monitor thread
        node_monitor_thread_ = std::thread(&L2FusionManager::node_monitor_thread_func, this);
        
        // Start Redis subscription
        start_redis_subscription();
        
        log_info("L2 Fusion Manager started with algorithm: " + algorithm_->get_name());
    }
    
    /**
     * @brief Stop the L2 fusion system
     */
    void stop() {
        if (!running_) {
            return;
        }
        
        running_ = false;
        queue_cv_.notify_all();
        
        // Wait for all threads to complete
        for (auto& thread : worker_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        if (algorithm_thread_.joinable()) {
            algorithm_thread_.join();
        }
        
        if (heartbeat_thread_.joinable()) {
            heartbeat_thread_.join();
        }
        
        if (node_monitor_thread_.joinable()) {
            node_monitor_thread_.join();
        }
        
        // Shutdown algorithm
        if (algorithm_) {
            algorithm_->shutdown(algorithm_context_);
        }
        
        log_info("L2 Fusion Manager stopped");
    }
    
    /**
     * @brief Send message to specific L1 node or broadcast
     */
    void send_to_l1(const messages::L2ToL1Message& message) {
        try {
            redis_messenger_->publish(config_.l2_to_l1_topic, message);
            messages_sent_++;
            
            log_debug("Sent message to L1 - Target: " + message.target_node_id() + 
                     ", Type: " + std::to_string(message.payload_case()));
        } catch (const std::exception& e) {
            log_error("Failed to send message to L1: " + std::string(e.what()));
        }
    }
    
    /**
     * @brief Get system statistics
     */
    struct SystemStats {
        uint64_t messages_processed;
        uint64_t messages_sent;
        size_t active_nodes;
        std::chrono::seconds uptime;
        std::string current_algorithm_state;
    };
    
    SystemStats get_stats() const {
        auto now = std::chrono::steady_clock::now();
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_);
        
        return SystemStats{
            .messages_processed = messages_processed_.load(),
            .messages_sent = messages_sent_.load(),
            .active_nodes = node_registry_.get_active_nodes(config_.node_timeout).size(),
            .uptime = uptime,
            .current_algorithm_state = algorithm_context_.current_state_name
        };
    }
    
    /**
     * @brief Get node registry (read-only access)
     */
    const NodeRegistry& get_node_registry() const {
        return node_registry_;
    }
    
    /**
     * @brief Trigger external event in algorithm
     */
    void trigger_algorithm_event(const std::string& trigger_name, const std::any& data = {}) {
        if (algorithm_) {
            algorithm_->handle_trigger(algorithm_context_, trigger_name, data);
        }
    }

private:
    void start_redis_subscription() {
        // Subscribe to L1 to L2 messages
        std::thread subscription_thread([this]() {
            redis_messenger_->subscribe<messages::L1ToL2Message>(
                config_.l1_to_l2_topic,
                [this](const messages::L1ToL2Message& message) {
                    handle_l1_message(message);
                }
            );
        });
        subscription_thread.detach();
    }
    
    void handle_l1_message(const messages::L1ToL2Message& message) {
        // Update node registry
        if (message.has_sender()) {
            node_registry_.register_node(message.sender());
        }
        
        // Handle different message types
        switch (message.payload_case()) {
            case messages::L1ToL2Message::kNodeStatus:
                node_registry_.update_node_status(message.sender().node_id(), message.node_status());
                break;
            case messages::L1ToL2Message::kHeartbeat:
                node_registry_.update_node_heartbeat(message.sender().node_id());
                break;
            default:
                // Queue message for algorithm processing
                enqueue_message(message);
                break;
        }
        
        log_debug("Received message from L1 node: " + message.sender().node_id());
    }
    
    void enqueue_message(const messages::L1ToL2Message& message) {
        std::unique_lock lock(queue_mutex_);
        
        if (message_queue_.size() >= config_.message_queue_size) {
            log_warning("Message queue full, dropping oldest message");
            message_queue_.pop();
        }
        
        message_queue_.push(message);
        queue_cv_.notify_one();
    }
    
    void worker_thread_func() {
        while (running_) {
            messages::L1ToL2Message message;
            
            {
                std::unique_lock lock(queue_mutex_);
                queue_cv_.wait(lock, [this] { return !message_queue_.empty() || !running_; });
                
                if (!running_) break;
                
                message = message_queue_.front();
                message_queue_.pop();
            }
            
            // Process message with algorithm
            try {
                algorithm_->process_l1_message(algorithm_context_, message);
                messages_processed_++;
                
                // Send any pending output messages
                send_pending_outputs();
                
            } catch (const std::exception& e) {
                log_error("Algorithm processing error: " + std::string(e.what()));
            }
        }
    }
    
    void algorithm_thread_func() {
        while (running_) {
            try {
                algorithm_->update(algorithm_context_);
                send_pending_outputs();
            } catch (const std::exception& e) {
                log_error("Algorithm update error: " + std::string(e.what()));
            }
            
            std::this_thread::sleep_for(config_.algorithm_update_interval);
        }
    }
    
    void heartbeat_thread_func() {
        while (running_) {
            send_heartbeat();
            std::this_thread::sleep_for(config_.heartbeat_interval);
        }
    }
    
    void node_monitor_thread_func() {
        while (running_) {
            auto timed_out_nodes = node_registry_.get_timed_out_nodes(config_.node_timeout);
            
            for (const auto& node_id : timed_out_nodes) {
                log_warning("Node timeout detected: " + node_id);
                algorithm_->handle_trigger(algorithm_context_, "node_timeout", node_id);
                node_registry_.remove_node(node_id);
            }
            
            std::this_thread::sleep_for(config_.node_timeout / 4);
        }
    }
    
    void send_pending_outputs() {
        for (const auto& message : algorithm_context_.pending_outputs) {
            send_to_l1(message);
        }
        algorithm_context_.pending_outputs.clear();
    }
    
    void send_heartbeat() {
        messages::L2ToL1Message heartbeat;
        heartbeat.set_message_id(generate_message_id());
        heartbeat.mutable_timestamp()->set_timestamp_ms(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
        
        // Add system status information
        auto* system_cmd = heartbeat.mutable_system_command();
        system_cmd->set_command_type(messages::SystemCommand::SYNC_TIME);
        
        try {
            redis_messenger_->publish(config_.heartbeat_topic, heartbeat);
        } catch (const std::exception& e) {
            log_error("Failed to send heartbeat: " + std::string(e.what()));
        }
    }
    
    std::string generate_message_id() {
        static std::atomic<uint64_t> counter{0};
        return "L2_" + std::to_string(counter++);
    }
    
    void log_debug(const std::string& message) {
        if (config_.enable_debug_logging) {
            std::cout << "[DEBUG] " << message << std::endl;
        }
    }
    
    void log_info(const std::string& message) {
        std::cout << "[INFO] " << message << std::endl;
    }
    
    void log_warning(const std::string& message) {
        std::cout << "[WARNING] " << message << std::endl;
    }
    
    void log_error(const std::string& message) {
        std::cerr << "[ERROR] " << message << std::endl;
    }
};

} // namespace dp_aero_l2::core