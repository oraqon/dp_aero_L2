#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <any>
#include <chrono>
#include <shared_mutex>
#include <mutex>
#include <optional>
#include <algorithm>

namespace dp_aero_l2::fusion {

// Forward declarations
class AlgorithmContext;

/**
 * @brief Task state machine for device-specific operations
 */
struct TaskState {
    std::string name;
    std::function<void(AlgorithmContext&, const std::string& task_id)> on_enter;
    std::function<void(AlgorithmContext&, const std::string& task_id)> on_exit;
    std::function<void(AlgorithmContext&, const std::string& task_id)> on_update;
    std::unordered_map<std::string, std::any> state_data;
    
    TaskState(const std::string& state_name) : name(state_name) {}
};

/**
 * @brief Task state transition definition
 */
struct TaskTransition {
    std::string from_state;
    std::string to_state;
    std::string trigger;
    std::function<bool(const AlgorithmContext&, const std::string& task_id)> condition;
    std::function<void(AlgorithmContext&, const std::string& task_id)> action;
    
    TaskTransition(const std::string& from, const std::string& to, const std::string& trigger_name)
        : from_state(from), to_state(to), trigger(trigger_name) {}
};

/**
 * @brief Task state machine manager for individual tasks
 */
class TaskStateMachine {
private:
    std::unordered_map<std::string, std::shared_ptr<TaskState>> states_;
    std::vector<TaskTransition> transitions_;
    std::string initial_state_;
    std::string current_state_;
    
public:
    void add_state(const std::string& name, std::shared_ptr<TaskState> state) {
        states_[name] = state;
        if (initial_state_.empty()) {
            initial_state_ = name;
            current_state_ = name;
        }
    }
    
    void add_transition(const TaskTransition& transition) {
        transitions_.push_back(transition);
    }
    
    void set_initial_state(const std::string& state_name) {
        initial_state_ = state_name;
        current_state_ = state_name;
    }
    
    std::shared_ptr<TaskState> get_state(const std::string& name) const {
        auto it = states_.find(name);
        return (it != states_.end()) ? it->second : nullptr;
    }
    
    std::string get_current_state() const { return current_state_; }
    std::string get_initial_state() const { return initial_state_; }
    
    bool try_transition(AlgorithmContext& context, const std::string& task_id, const std::string& trigger) {
        for (const auto& transition : transitions_) {
            if (transition.from_state == current_state_ && 
                transition.trigger == trigger &&
                (!transition.condition || transition.condition(context, task_id))) {
                
                // Exit current state
                auto current_state_obj = get_state(current_state_);
                if (current_state_obj && current_state_obj->on_exit) {
                    current_state_obj->on_exit(context, task_id);
                }
                
                // Execute transition action
                if (transition.action) {
                    transition.action(context, task_id);
                }
                
                // Enter new state
                current_state_ = transition.to_state;
                auto new_state_obj = get_state(transition.to_state);
                
                if (new_state_obj && new_state_obj->on_enter) {
                    new_state_obj->on_enter(context, task_id);
                }
                
                return true;
            }
        }
        return false;
    }
    
    void update(AlgorithmContext& context, const std::string& task_id) {
        auto current_state_obj = get_state(current_state_);
        if (current_state_obj && current_state_obj->on_update) {
            current_state_obj->on_update(context, task_id);
        }
    }
    
    const std::vector<TaskTransition>& get_transitions() const { return transitions_; }
};

/**
 * @brief Represents a task assigned to a device for a specific target
 */
class Task {
public:
    enum class Type {
        TRACK_TARGET,      // Track a specific target
        SCAN_AREA,         // Scan for new targets in area
        POINT_GIMBAL,      // Point gimbal at specific coordinates
        CALIBRATE_SENSOR,  // Perform sensor calibration
        MONITOR_STATUS     // Monitor device health
    };
    
    enum class Priority {
        LOW = 1,
        NORMAL = 5,
        HIGH = 8,
        CRITICAL = 10
    };
    
    enum class Status {
        CREATED,       // Task created but not assigned
        ASSIGNED,      // Assigned to device but not started
        ACTIVE,        // Currently executing
        PAUSED,        // Temporarily paused
        COMPLETED,     // Successfully completed
        FAILED,        // Failed to execute
        CANCELLED      // Cancelled by user/system
    };

private:
    std::string task_id_;
    std::string target_id_;
    std::string device_id_;
    Type type_;
    Priority priority_;
    Status status_;
    
    std::chrono::steady_clock::time_point created_time_;
    std::chrono::steady_clock::time_point assigned_time_;
    std::chrono::steady_clock::time_point started_time_;
    std::chrono::steady_clock::time_point completed_time_;
    
    std::unordered_map<std::string, std::any> parameters_;
    std::unique_ptr<TaskStateMachine> state_machine_;
    
    // Progress tracking
    float progress_percentage_{0.0f};
    std::string status_message_;
    
public:
    Task(const std::string& task_id, const std::string& target_id, Type type, Priority priority = Priority::NORMAL)
        : task_id_(task_id), target_id_(target_id), type_(type), priority_(priority), 
          status_(Status::CREATED), created_time_(std::chrono::steady_clock::now()),
          state_machine_(std::make_unique<TaskStateMachine>()) {
        setup_default_state_machine();
    }
    
    // Getters
    const std::string& get_task_id() const { return task_id_; }
    const std::string& get_target_id() const { return target_id_; }
    const std::string& get_device_id() const { return device_id_; }
    Type get_type() const { return type_; }
    Priority get_priority() const { return priority_; }
    Status get_status() const { return status_; }
    float get_progress() const { return progress_percentage_; }
    const std::string& get_status_message() const { return status_message_; }
    
    // Time getters
    std::chrono::steady_clock::time_point get_created_time() const { return created_time_; }
    std::chrono::steady_clock::time_point get_assigned_time() const { return assigned_time_; }
    std::chrono::steady_clock::time_point get_started_time() const { return started_time_; }
    std::chrono::steady_clock::time_point get_completed_time() const { return completed_time_; }
    
    // Setters
    void set_device_id(const std::string& device_id) { 
        device_id_ = device_id; 
        if (status_ == Status::CREATED) {
            status_ = Status::ASSIGNED;
            assigned_time_ = std::chrono::steady_clock::now();
        }
    }
    
    void set_status(Status status) { 
        status_ = status; 
        auto now = std::chrono::steady_clock::now();
        
        switch (status) {
            case Status::ACTIVE:
                if (started_time_ == std::chrono::steady_clock::time_point{}) {
                    started_time_ = now;
                }
                break;
            case Status::COMPLETED:
            case Status::FAILED:
            case Status::CANCELLED:
                completed_time_ = now;
                progress_percentage_ = (status == Status::COMPLETED) ? 100.0f : progress_percentage_;
                break;
            default:
                break;
        }
    }
    
    void set_priority(Priority priority) { priority_ = priority; }
    void set_progress(float percentage) { progress_percentage_ = std::clamp(percentage, 0.0f, 100.0f); }
    void set_status_message(const std::string& message) { status_message_ = message; }
    
    // Parameter management
    template<typename T>
    void set_parameter(const std::string& key, const T& value) {
        parameters_[key] = value;
    }
    
    template<typename T>
    std::optional<T> get_parameter(const std::string& key) const {
        auto it = parameters_.find(key);
        if (it != parameters_.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
    
    // State machine operations
    TaskStateMachine* get_state_machine() { return state_machine_.get(); }
    const TaskStateMachine* get_state_machine() const { return state_machine_.get(); }
    
    bool trigger_state_transition(AlgorithmContext& context, const std::string& trigger) {
        return state_machine_->try_transition(context, task_id_, trigger);
    }
    
    void update_state_machine(AlgorithmContext& context) {
        state_machine_->update(context, task_id_);
    }
    
    // Utility methods
    bool is_active() const { return status_ == Status::ACTIVE; }
    bool is_completed() const { return status_ == Status::COMPLETED || status_ == Status::FAILED || status_ == Status::CANCELLED; }
    
    std::chrono::milliseconds get_age() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - created_time_);
    }
    
    std::chrono::milliseconds get_execution_time() const {
        if (started_time_ == std::chrono::steady_clock::time_point{}) {
            return std::chrono::milliseconds::zero();
        }
        
        auto end_time = (completed_time_ != std::chrono::steady_clock::time_point{}) ? 
                       completed_time_ : std::chrono::steady_clock::now();
        
        return std::chrono::duration_cast<std::chrono::milliseconds>(end_time - started_time_);
    }
    
    static std::string type_to_string(Type type) {
        switch (type) {
            case Type::TRACK_TARGET: return "TRACK_TARGET";
            case Type::SCAN_AREA: return "SCAN_AREA";
            case Type::POINT_GIMBAL: return "POINT_GIMBAL";
            case Type::CALIBRATE_SENSOR: return "CALIBRATE_SENSOR";
            case Type::MONITOR_STATUS: return "MONITOR_STATUS";
            default: return "UNKNOWN";
        }
    }
    
    static std::string status_to_string(Status status) {
        switch (status) {
            case Status::CREATED: return "CREATED";
            case Status::ASSIGNED: return "ASSIGNED";
            case Status::ACTIVE: return "ACTIVE";
            case Status::PAUSED: return "PAUSED";
            case Status::COMPLETED: return "COMPLETED";
            case Status::FAILED: return "FAILED";
            case Status::CANCELLED: return "CANCELLED";
            default: return "UNKNOWN";
        }
    }

private:
    void setup_default_state_machine() {
        // Create default task states
        auto initializing_state = std::make_shared<TaskState>("INITIALIZING");
        initializing_state->on_enter = [](AlgorithmContext& ctx, const std::string& task_id) {
            // Initialization logic
        };
        
        auto executing_state = std::make_shared<TaskState>("EXECUTING");
        executing_state->on_update = [](AlgorithmContext& ctx, const std::string& task_id) {
            // Main task execution logic
        };
        
        auto completing_state = std::make_shared<TaskState>("COMPLETING");
        completing_state->on_enter = [](AlgorithmContext& ctx, const std::string& task_id) {
            // Cleanup and completion logic
        };
        
        auto error_state = std::make_shared<TaskState>("ERROR");
        error_state->on_enter = [](AlgorithmContext& ctx, const std::string& task_id) {
            // Error handling logic
        };
        
        // Add states
        state_machine_->add_state("INITIALIZING", initializing_state);
        state_machine_->add_state("EXECUTING", executing_state);
        state_machine_->add_state("COMPLETING", completing_state);
        state_machine_->add_state("ERROR", error_state);
        
        // Set initial state
        state_machine_->set_initial_state("INITIALIZING");
        
        // Define transitions
        state_machine_->add_transition(TaskTransition("INITIALIZING", "EXECUTING", "start"));
        state_machine_->add_transition(TaskTransition("EXECUTING", "COMPLETING", "complete"));
        state_machine_->add_transition(TaskTransition("INITIALIZING", "ERROR", "error"));
        state_machine_->add_transition(TaskTransition("EXECUTING", "ERROR", "error"));
        state_machine_->add_transition(TaskTransition("ERROR", "INITIALIZING", "retry"));
    }
};

/**
 * @brief Manages assignments between targets, devices, and tasks
 */
class TaskManager {
private:
    mutable std::shared_mutex mutex_;
    
    // Core mappings
    std::unordered_map<std::string, std::unique_ptr<Task>> tasks_;                    // task_id -> Task
    std::unordered_map<std::string, std::vector<std::string>> target_to_tasks_;      // target_id -> task_ids
    std::unordered_map<std::string, std::vector<std::string>> device_to_tasks_;      // device_id -> task_ids
    
    // Assignment tracking
    std::unordered_map<std::string, std::string> target_primary_device_;             // target_id -> primary_device_id
    std::unordered_map<std::string, std::vector<std::string>> device_capabilities_;  // device_id -> capabilities
    
    // Statistics
    uint64_t next_task_id_{1};
    std::chrono::steady_clock::time_point last_cleanup_time_;
    
public:
    TaskManager() : last_cleanup_time_(std::chrono::steady_clock::now()) {}
    
    /**
     * @brief Create a new task for a target
     */
    std::string create_task(const std::string& target_id, Task::Type type, Task::Priority priority = Task::Priority::NORMAL) {
        std::unique_lock lock(mutex_);
        
        std::string task_id = "task_" + std::to_string(next_task_id_++);
        auto task = std::make_unique<Task>(task_id, target_id, type, priority);
        
        tasks_[task_id] = std::move(task);
        target_to_tasks_[target_id].push_back(task_id);
        
        return task_id;
    }
    
    /**
     * @brief Assign a task to a specific device
     */
    bool assign_task_to_device(const std::string& task_id, const std::string& device_id) {
        std::unique_lock lock(mutex_);
        
        auto task_it = tasks_.find(task_id);
        if (task_it == tasks_.end()) {
            return false;
        }
        
        auto& task = task_it->second;
        
        // Remove from previous device assignment if exists
        if (!task->get_device_id().empty()) {
            auto& prev_device_tasks = device_to_tasks_[task->get_device_id()];
            prev_device_tasks.erase(
                std::remove(prev_device_tasks.begin(), prev_device_tasks.end(), task_id),
                prev_device_tasks.end()
            );
        }
        
        // Assign to new device
        task->set_device_id(device_id);
        device_to_tasks_[device_id].push_back(task_id);
        
        // Update primary device mapping for target
        target_primary_device_[task->get_target_id()] = device_id;
        
        return true;
    }
    
    /**
     * @brief Get task by ID
     */
    Task* get_task(const std::string& task_id) {
        std::shared_lock lock(mutex_);
        auto it = tasks_.find(task_id);
        return (it != tasks_.end()) ? it->second.get() : nullptr;
    }
    
    const Task* get_task(const std::string& task_id) const {
        std::shared_lock lock(mutex_);
        auto it = tasks_.find(task_id);
        return (it != tasks_.end()) ? it->second.get() : nullptr;
    }
    
    /**
     * @brief Get all tasks for a target
     */
    std::vector<Task*> get_tasks_for_target(const std::string& target_id) {
        std::shared_lock lock(mutex_);
        std::vector<Task*> result;
        
        auto it = target_to_tasks_.find(target_id);
        if (it != target_to_tasks_.end()) {
            for (const auto& task_id : it->second) {
                auto task_it = tasks_.find(task_id);
                if (task_it != tasks_.end()) {
                    result.push_back(task_it->second.get());
                }
            }
        }
        
        return result;
    }
    
    /**
     * @brief Get all tasks for a device
     */
    std::vector<Task*> get_tasks_for_device(const std::string& device_id) {
        std::shared_lock lock(mutex_);
        std::vector<Task*> result;
        
        auto it = device_to_tasks_.find(device_id);
        if (it != device_to_tasks_.end()) {
            for (const auto& task_id : it->second) {
                auto task_it = tasks_.find(task_id);
                if (task_it != tasks_.end()) {
                    result.push_back(task_it->second.get());
                }
            }
        }
        
        return result;
    }
    
    /**
     * @brief Get primary device assigned to a target
     */
    std::optional<std::string> get_primary_device_for_target(const std::string& target_id) const {
        std::shared_lock lock(mutex_);
        auto it = target_primary_device_.find(target_id);
        return (it != target_primary_device_.end()) ? std::make_optional(it->second) : std::nullopt;
    }
    
    /**
     * @brief Register device capabilities
     */
    void register_device_capabilities(const std::string& device_id, const std::vector<std::string>& capabilities) {
        std::unique_lock lock(mutex_);
        device_capabilities_[device_id] = capabilities;
    }
    
    /**
     * @brief Get device capabilities
     */
    std::vector<std::string> get_device_capabilities(const std::string& device_id) const {
        std::shared_lock lock(mutex_);
        auto it = device_capabilities_.find(device_id);
        return (it != device_capabilities_.end()) ? it->second : std::vector<std::string>{};
    }
    
    /**
     * @brief Remove task
     */
    bool remove_task(const std::string& task_id) {
        std::unique_lock lock(mutex_);
        
        auto task_it = tasks_.find(task_id);
        if (task_it == tasks_.end()) {
            return false;
        }
        
        const auto& task = task_it->second;
        const std::string& target_id = task->get_target_id();
        const std::string& device_id = task->get_device_id();
        
        // Remove from target mapping
        auto target_it = target_to_tasks_.find(target_id);
        if (target_it != target_to_tasks_.end()) {
            auto& task_list = target_it->second;
            task_list.erase(std::remove(task_list.begin(), task_list.end(), task_id), task_list.end());
            
            if (task_list.empty()) {
                target_to_tasks_.erase(target_it);
                target_primary_device_.erase(target_id);
            }
        }
        
        // Remove from device mapping
        if (!device_id.empty()) {
            auto device_it = device_to_tasks_.find(device_id);
            if (device_it != device_to_tasks_.end()) {
                auto& task_list = device_it->second;
                task_list.erase(std::remove(task_list.begin(), task_list.end(), task_id), task_list.end());
                
                if (task_list.empty()) {
                    device_to_tasks_.erase(device_it);
                }
            }
        }
        
        // Remove task itself
        tasks_.erase(task_it);
        return true;
    }
    
    /**
     * @brief Update all active tasks
     */
    void update_all_tasks(AlgorithmContext& context) {
        std::shared_lock lock(mutex_);
        
        for (auto& [task_id, task] : tasks_) {
            if (task->is_active()) {
                task->update_state_machine(context);
            }
        }
        
        // Periodic cleanup
        auto now = std::chrono::steady_clock::now();
        if (now - last_cleanup_time_ > std::chrono::minutes(5)) {
            // Note: This would need to be done with unique_lock, but keeping simple for now
            // cleanup_completed_tasks();
            last_cleanup_time_ = now;
        }
    }
    
    /**
     * @brief Get all active tasks
     */
    std::vector<Task*> get_active_tasks() {
        std::shared_lock lock(mutex_);
        std::vector<Task*> result;
        
        for (auto& [task_id, task] : tasks_) {
            if (task->is_active()) {
                result.push_back(task.get());
            }
        }
        
        return result;
    }
    
    /**
     * @brief Get task statistics
     */
    struct TaskStats {
        size_t total_tasks = 0;
        size_t active_tasks = 0;
        size_t completed_tasks = 0;
        size_t failed_tasks = 0;
        size_t registered_devices = 0;
        size_t targets_with_assignments = 0;
    };
    
    TaskStats get_task_statistics() const {
        std::shared_lock lock(mutex_);
        TaskStats stats;
        
        stats.total_tasks = tasks_.size();
        stats.registered_devices = device_capabilities_.size();
        stats.targets_with_assignments = target_primary_device_.size();
        
        for (const auto& [task_id, task] : tasks_) {
            switch (task->get_status()) {
                case Task::Status::ACTIVE:
                    stats.active_tasks++;
                    break;
                case Task::Status::COMPLETED:
                    stats.completed_tasks++;
                    break;
                case Task::Status::FAILED:
                case Task::Status::CANCELLED:
                    stats.failed_tasks++;
                    break;
                default:
                    break;
            }
        }
        
        return stats;
    }
    
    /**
     * @brief Clear all tasks and assignments
     */
    void clear_all() {
        std::unique_lock lock(mutex_);
        tasks_.clear();
        target_to_tasks_.clear();
        device_to_tasks_.clear();
        target_primary_device_.clear();
        // Keep device_capabilities_ as they represent persistent device info
    }

private:
    void cleanup_completed_tasks() {
        // Remove completed tasks older than 1 hour
        auto cutoff_time = std::chrono::steady_clock::now() - std::chrono::hours(1);
        
        std::vector<std::string> tasks_to_remove;
        for (const auto& [task_id, task] : tasks_) {
            if (task->is_completed() && task->get_completed_time() < cutoff_time) {
                tasks_to_remove.push_back(task_id);
            }
        }
        
        for (const auto& task_id : tasks_to_remove) {
            remove_task(task_id);
        }
    }
};

} // namespace dp_aero_l2::fusion