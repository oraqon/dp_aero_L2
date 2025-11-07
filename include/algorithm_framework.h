#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <any>
#include <chrono>
#include <concepts>
#include <shared_mutex>
#include <optional>
#include <queue>

#include "messages/l1_to_l2.pb.h"
#include "messages/l2_to_l1.pb.h"
#include "task_manager.h"

namespace dp_aero_l2::fusion {

// Forward declarations
class AlgorithmContext;
class StateManager;

/**
 * @brief State machine state representation
 */
struct State {
    std::string name;
    std::function<void(AlgorithmContext&)> on_enter;
    std::function<void(AlgorithmContext&)> on_exit;
    std::function<void(AlgorithmContext&)> on_update;
    std::unordered_map<std::string, std::any> state_data;
    
    State(const std::string& state_name) : name(state_name) {}
};

/**
 * @brief State transition definition
 */
struct Transition {
    std::string from_state;
    std::string to_state;
    std::string trigger;
    std::function<bool(const AlgorithmContext&)> condition;
    std::function<void(AlgorithmContext&)> action;
    
    Transition(const std::string& from, const std::string& to, const std::string& trigger_name)
        : from_state(from), to_state(to), trigger(trigger_name) {}
};

/**
 * @brief Algorithm execution context containing state and data
 */
class AlgorithmContext {
public:
    // Current algorithm state
    std::string current_state_name;
    std::shared_ptr<State> current_state;
    
    // Input data from L1 nodes
    std::unordered_map<std::string, messages::L1ToL2Message> latest_l1_messages;
    std::unordered_map<std::string, std::vector<messages::L1ToL2Message>> message_history;
    
    // Algorithm-specific data storage
    std::unordered_map<std::string, std::any> algorithm_data;
    
    // Timing information
    std::chrono::steady_clock::time_point last_update;
    std::chrono::milliseconds update_interval{100};
    
    // Output messages to be sent to L1 nodes
    std::vector<messages::L2ToL1Message> pending_outputs;
    
    // Helper methods
    template<typename T>
    void set_data(const std::string& key, const T& value) {
        algorithm_data[key] = value;
    }
    
    template<typename T>
    std::optional<T> get_data(const std::string& key) const {
        auto it = algorithm_data.find(key);
        if (it != algorithm_data.end()) {
            try {
                return std::any_cast<T>(it->second);
            } catch (const std::bad_any_cast&) {
                return std::nullopt;
            }
        }
        return std::nullopt;
    }
    
    void add_output_message(const messages::L2ToL1Message& message) {
        pending_outputs.push_back(message);
    }
    
    std::vector<messages::L1ToL2Message> get_messages_from_node(const std::string& node_id) const {
        auto it = message_history.find(node_id);
        return (it != message_history.end()) ? it->second : std::vector<messages::L1ToL2Message>{};
    }
};

/**
 * @brief State machine manager
 */
class StateManager {
private:
    std::unordered_map<std::string, std::shared_ptr<State>> states_;
    std::vector<Transition> transitions_;
    std::string initial_state_;
    
public:
    void add_state(const std::string& name, std::shared_ptr<State> state) {
        states_[name] = state;
        if (initial_state_.empty()) {
            initial_state_ = name;
        }
    }
    
    void add_transition(const Transition& transition) {
        transitions_.push_back(transition);
    }
    
    void set_initial_state(const std::string& state_name) {
        initial_state_ = state_name;
    }
    
    std::shared_ptr<State> get_state(const std::string& name) const {
        auto it = states_.find(name);
        return (it != states_.end()) ? it->second : nullptr;
    }
    
    std::string get_initial_state() const { return initial_state_; }
    
    bool try_transition(AlgorithmContext& context, const std::string& trigger) {
        for (const auto& transition : transitions_) {
            if (transition.from_state == context.current_state_name && 
                transition.trigger == trigger &&
                (!transition.condition || transition.condition(context))) {
                
                // Exit current state
                if (context.current_state && context.current_state->on_exit) {
                    context.current_state->on_exit(context);
                }
                
                // Execute transition action
                if (transition.action) {
                    transition.action(context);
                }
                
                // Enter new state
                context.current_state_name = transition.to_state;
                context.current_state = get_state(transition.to_state);
                
                if (context.current_state && context.current_state->on_enter) {
                    context.current_state->on_enter(context);
                }
                
                return true;
            }
        }
        return false;
    }
    
    const std::vector<Transition>& get_transitions() const { return transitions_; }
};

/**
 * @brief Abstract base class for fusion algorithms
 */
class FusionAlgorithm {
public:
    virtual ~FusionAlgorithm() = default;
    
    /**
     * @brief Initialize the algorithm and set up state machine
     * @param context Algorithm execution context
     */
    virtual void initialize(AlgorithmContext& context) = 0;
    
    /**
     * @brief Process new message from L1 node
     * @param context Algorithm execution context
     * @param message Incoming message from L1 node
     */
    virtual void process_l1_message(AlgorithmContext& context, 
                                   const messages::L1ToL2Message& message) = 0;
    
    /**
     * @brief Periodic update call (based on update_interval)
     * @param context Algorithm execution context
     */
    virtual void update(AlgorithmContext& context) = 0;
    
    /**
     * @brief Handle external triggers/events
     * @param context Algorithm execution context
     * @param trigger_name Name of the trigger
     * @param trigger_data Optional trigger data
     */
    virtual void handle_trigger(AlgorithmContext& context, 
                               const std::string& trigger_name,
                               const std::any& trigger_data = {}) = 0;
    
    /**
     * @brief Get algorithm name/identifier
     */
    virtual std::string get_name() const = 0;
    
    /**
     * @brief Get algorithm version
     */
    virtual std::string get_version() const = 0;
    
    /**
     * @brief Get algorithm description
     */
    virtual std::string get_description() const = 0;
    
    /**
     * @brief Shutdown and cleanup
     * @param context Algorithm execution context
     */
    virtual void shutdown(AlgorithmContext& context) = 0;
    
protected:
    StateManager state_manager_;
    TaskManager task_manager_;
    
    /**
     * @brief Helper to create and configure the state machine
     */
    virtual void setup_state_machine() = 0;
    
    /**
     * @brief Helper to trigger state transitions
     */
    bool trigger_transition(AlgorithmContext& context, const std::string& trigger) {
        return state_manager_.try_transition(context, trigger);
    }
    
    /**
     * @brief Helper to add states to the state machine
     */
    void add_state(const std::string& name, std::shared_ptr<State> state) {
        state_manager_.add_state(name, state);
    }
    
    /**
     * @brief Helper to add transitions to the state machine
     */
    void add_transition(const Transition& transition) {
        state_manager_.add_transition(transition);
    }
    
    /**
     * @brief Helper to set initial state
     */
    void set_initial_state(const std::string& state_name) {
        state_manager_.set_initial_state(state_name);
    }
    
    /**
     * @brief Get task manager for target-device-task assignments
     */
    TaskManager& get_task_manager() {
        return task_manager_;
    }
    
    const TaskManager& get_task_manager() const {
        return task_manager_;
    }
    
    /**
     * @brief Helper to create a task for a target
     */
    std::string create_task_for_target(const std::string& target_id, Task::Type type, Task::Priority priority = Task::Priority::NORMAL) {
        return task_manager_.create_task(target_id, type, priority);
    }
    
    /**
     * @brief Helper to assign task to device
     */
    bool assign_task_to_device(const std::string& task_id, const std::string& device_id) {
        return task_manager_.assign_task_to_device(task_id, device_id);
    }
    
    /**
     * @brief Helper to update all tasks
     */
    void update_all_tasks(AlgorithmContext& context) {
        task_manager_.update_all_tasks(context);
    }
};

/**
 * @brief Algorithm factory interface for plugin-style loading
 */
class AlgorithmFactory {
public:
    virtual ~AlgorithmFactory() = default;
    virtual std::unique_ptr<FusionAlgorithm> create_algorithm() = 0;
    virtual std::string get_algorithm_name() const = 0;
    virtual std::string get_algorithm_version() const = 0;
};

/**
 * @brief Template helper for creating algorithm factories
 */
template<typename AlgorithmType>
requires std::derived_from<AlgorithmType, FusionAlgorithm>
class TypedAlgorithmFactory : public AlgorithmFactory {
public:
    std::unique_ptr<FusionAlgorithm> create_algorithm() override {
        return std::make_unique<AlgorithmType>();
    }
    
    std::string get_algorithm_name() const override {
        auto temp_algorithm = std::make_unique<AlgorithmType>();
        return temp_algorithm->get_name();
    }
    
    std::string get_algorithm_version() const override {
        auto temp_algorithm = std::make_unique<AlgorithmType>();
        return temp_algorithm->get_version();
    }
};

/**
 * @brief Algorithm registry for managing multiple algorithms
 */
class AlgorithmRegistry {
private:
    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, std::unique_ptr<AlgorithmFactory>> factories_;
    
public:
    template<typename AlgorithmType>
    requires std::derived_from<AlgorithmType, FusionAlgorithm>
    void register_algorithm() {
        std::unique_lock lock(mutex_);
        auto factory = std::make_unique<TypedAlgorithmFactory<AlgorithmType>>();
        auto name = factory->get_algorithm_name();
        factories_[name] = std::move(factory);
    }
    
    std::unique_ptr<FusionAlgorithm> create_algorithm(const std::string& name) const {
        std::shared_lock lock(mutex_);
        auto it = factories_.find(name);
        if (it != factories_.end()) {
            return it->second->create_algorithm();
        }
        return nullptr;
    }
    
    std::vector<std::string> get_available_algorithms() const {
        std::shared_lock lock(mutex_);
        std::vector<std::string> names;
        for (const auto& [name, factory] : factories_) {
            names.push_back(name);
        }
        return names;
    }
    
    bool is_algorithm_available(const std::string& name) const {
        std::shared_lock lock(mutex_);
        return factories_.find(name) != factories_.end();
    }
};

} // namespace dp_aero_l2::fusion