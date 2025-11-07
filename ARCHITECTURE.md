# L2 Fusion System Architecture

## Overview

The L2 Fusion System is a modular, scalable framework for fusing data from multiple L1 sensor nodes in aerospace applications. It implements a generic algorithm framework with state machine support, allowing for easy extension and customization of fusion algorithms.

## System Architecture

```
┌─────────────────┐    Redis L1→L2    ┌─────────────────┐
│   L1 Sensor     │ ───────────────► │                 │
│   Node (Radar)  │                  │                 │
└─────────────────┘                  │                 │
                                     │                 │
┌─────────────────┐    Redis L1→L2    │                 │
│   L1 Sensor     │ ───────────────► │   L2 Fusion     │
│   Node (LiDAR)  │                  │     System      │
└─────────────────┘                  │                 │
                                     │                 │
┌─────────────────┐    Redis L1→L2    │                 │
│   L1 Sensor     │ ───────────────► │                 │
│   Node (Camera) │                  │                 │
└─────────────────┘                  └─────────────────┘
                                             │
                                             │ Redis L2→L1
                                             ▼
                                    ┌─────────────────┐
                                    │   Ground        │
                                    │   Control       │
                                    │   Station       │
                                    └─────────────────┘
```

## Key Components

### 1. **Algorithm Framework** (`include/algorithm_framework.h`)
### This is the **Target Manager**

The heart of the system - provides a generic interface for fusion algorithms with built-in state machine support.

**Key Features:**
- **Abstract Base Class**: `FusionAlgorithm` - implement your custom logic
- **State Machine**: Built-in state management with transitions and triggers  
- **Algorithm Context**: Shared data store and message history
- **Plugin Architecture**: Dynamic algorithm loading via registry
- **Type Safety**: Template-based with C++20 concepts
- **Task Manager Integration**: Built-in TaskManager for target-device-task assignments

### 1.1 **Task Manager** (`include/task_manager.h`)
### This is the **Assignment Coordinator**

Manages the mapping between targets, devices, and tasks. Preparation for multi-device coordination.

**Key Features:**
- **Target-Device-Task Mapping**: Maintains relationships between all three entities
- **Task State Machines**: Each task has its own state machine (INITIALIZING → EXECUTING → COMPLETING)
- **Device Capability Registry**: Tracks what each device can do
- **Priority-Based Assignment**: Tasks have priority levels (LOW, NORMAL, HIGH, CRITICAL)
- **Single Device Mode**: Currently assigns all tasks to one device (preparation for multi-device)

**Task Types:**
- `TRACK_TARGET`: Track a specific target
- `SCAN_AREA`: Scan for new targets in area  
- `POINT_GIMBAL`: Point gimbal at specific coordinates
- `CALIBRATE_SENSOR`: Perform sensor calibration
- `MONITOR_STATUS`: Monitor device health

**Example Algorithm States:**
```cpp
// Example: Target Tracking Algorithm
IDLE → ACQUIRING → TRACKING → LOST → IDLE
  ↑                             ↓
  └─────────── RESET ───────────┘
```

**Example Task Management (First Demo - Single Device):**
```cpp
// During algorithm initialization
std::string device_id = "default_device";
std::vector<std::string> capabilities = {"radar", "lidar", "camera", "gimbal_control"};
get_task_manager().register_device_capabilities(device_id, capabilities);

// When new target detected
std::string target_id = "target_001";
std::string task_id = create_task_for_target(target_id, Task::Type::TRACK_TARGET, Task::Priority::HIGH);
assign_task_to_device(task_id, device_id);

// During algorithm update
update_all_tasks(context); // Updates all task state machines
```

### 2. **L2 Fusion Manager** (`include/l2_fusion_manager.h`)
### Later (after demo #1): Will become the **Device Manager** 

Main system orchestrator that handles Redis communication, node management, and algorithm execution.

**Key Features:**
- **Multi-threaded**: Separate threads for algorithm, communication, and monitoring
- **Node Registry**: Tracks L1 nodes with timeout detection and health monitoring
- **Message Queue**: Buffered processing with configurable queue size
- **Statistics**: Real-time performance monitoring and reporting
- **Error Handling**: Robust error recovery and logging
- **Later (after demo #1):** Will do device management (based on target priority and device availability)

### 3. **Protocol Buffers Messages**

Strongly-typed message definitions for L1↔L2 communication:

- **L1→L2 Messages** (`proto/messages/l1_to_l2.proto`):
  - Sensor data (radar, lidar, camera, IMU, GPS)
  - Node status and heartbeat
  - Capability advertisements

- **L2→L1 Messages** (`proto/messages/l2_to_l1.proto`):
  - Control commands (gimbal pointing, sensor configuration)
  - Fusion results
  - System commands

### 4. **Redis Integration** (`include/redis_utils.h`)

Type-safe Redis communication layer supporting multiple patterns:
- **Pub/Sub**: Real-time message broadcasting
- **Streams**: Persistent message logs with replay capability  
- **Queues**: Task distribution with FIFO semantics

## Implementation Example

### Creating a Custom Algorithm

```cpp
#include "algorithm_framework.h"

class MyFusionAlgorithm : public fusion::FusionAlgorithm {
public:
    std::string get_name() const override { return "MyFusionAlgorithm"; }
    std::string get_version() const override { return "1.0.0"; }
    std::string get_description() const override { return "My custom fusion logic"; }
    
    void initialize(fusion::AlgorithmContext& context) override {
        setup_state_machine();
        // Initialize your algorithm state
        context.set_data<int>("detection_count", 0);
    }
    
    void process_l1_message(fusion::AlgorithmContext& context, 
                           const messages::L1ToL2Message& message) override {
        // Process incoming sensor data
        if (message.has_sensor_data()) {
            // Your fusion logic here
            auto count = context.get_data<int>("detection_count").value_or(0);
            context.set_data("detection_count", count + 1);
            
            // Trigger state transition if needed
            if (count > 10) {
                handle_trigger(context, "enough_data");
            }
        }
    }
    
    void update(fusion::AlgorithmContext& context) override {
        // Periodic algorithm updates
        if (context.current_state && context.current_state->on_update) {
            context.current_state->on_update(context);
        }
    }
    
    // ... implement other required methods

protected:
    void setup_state_machine() override {
        // Define your states
        auto idle_state = std::make_shared<fusion::State>("IDLE");
        idle_state->on_enter = [](fusion::AlgorithmContext& ctx) {
            std::cout << "Entered IDLE state\n";
        };
        
        auto processing_state = std::make_shared<fusion::State>("PROCESSING");
        // ... configure state
        
        // Add states and transitions
        add_state("IDLE", idle_state);
        add_state("PROCESSING", processing_state);
        add_transition(fusion::Transition("IDLE", "PROCESSING", "enough_data"));
        set_initial_state("IDLE");
    }
};
```

### Registering and Using Your Algorithm

```cpp
int main() {
    // Create algorithm registry
    fusion::AlgorithmRegistry registry;
    registry.register_algorithm<MyFusionAlgorithm>();
    
    // Create L2 system
    core::L2FusionManager fusion_manager;
    auto algorithm = registry.create_algorithm("MyFusionAlgorithm");
    fusion_manager.set_algorithm(std::move(algorithm));
    
    // Start system
    fusion_manager.start();
    
    // System runs until stopped...
}
```

## Message Flow

### 1. **L1 Node Registration**
```
L1 Node → Redis → L2 System
        (capability advertisement)
```

### 2. **Sensor Data Flow**
```
L1 Sensor → Redis Pub/Sub → L2 Queue → Algorithm Processing → Redis Pub/Sub → L1 Commands
```

### 3. **State Management**
```
Sensor Data → Algorithm Logic → State Transitions → Output Commands
```

## Configuration

### L2 System Configuration
```cpp
core::L2Config config;
config.redis_connection = "tcp://127.0.0.1:6379";
config.algorithm_name = "TargetTrackingAlgorithm";
config.worker_threads = 4;
config.algorithm_update_interval = std::chrono::milliseconds(50);
config.node_timeout = std::chrono::seconds(30);
```

### L1 Node Configuration
```bash
./l1_node_simulator \
    --node-id "radar_001" \
    --node-type "radar" \
    --location "nose_cone" \
    --interval 100 \
    --detection-prob 0.4
```

## State Machine Design

The framework provides a flexible state machine implementation:

### States
- **Enter/Exit Actions**: Code executed when entering/leaving states
- **Update Actions**: Periodic actions while in state  
- **State Data**: Per-state data storage

### Transitions  
- **Triggers**: Named events that can cause transitions
- **Conditions**: Boolean functions that must be true for transition
- **Actions**: Code executed during transition

### Example State Machine Flow
```
┌─────────┐  sensor_data   ┌─────────────┐  confirmed   ┌──────────┐
│  IDLE   │ ─────────────► │  ACQUIRING  │ ───────────► │ TRACKING │
└─────────┘                └─────────────┘              └──────────┘
     ▲                           │                            │
     │                           │ false_positive             │ target_lost
     │                           ▼                            ▼
     │                      ┌─────────┐                  ┌────────┐
     └──────────────────────│  RESET  │◄─────────────────│  LOST  │
                           └─────────┘     timeout       └────────┘
```

## Key Benefits

### **Modularity**
- **Pluggable Algorithms**: Easy to swap fusion algorithms
- **Independent L1 Nodes**: Nodes can join/leave dynamically
- **Configurable Behavior**: Runtime configuration without code changes

### **Scalability** 
- **Multi-threading**: Parallel processing of sensor data
- **Redis Clustering**: Horizontal scaling support
- **Queue Management**: Handles burst traffic and load balancing

### **Reliability**
- **Node Timeout Detection**: Automatic handling of failed nodes
- **Error Recovery**: Graceful degradation and error reporting
- **State Persistence**: Algorithm state survives restarts (with Redis Streams)

### **Extensibility**
- **Generic Framework**: Support any fusion algorithm
- **Message Types**: Easy to add new sensor types
- **Protocol Evolution**: Protobuf ensures backward compatibility

## Performance Characteristics

- **Latency**: < 10ms typical processing latency
- **Throughput**: > 1000 messages/second per L1 node
- **Memory**: ~50MB base + algorithm-specific requirements
- **CPU**: Scales linearly with number of sensor nodes

## Testing and Simulation

The system includes comprehensive simulation tools:

### **L1 Node Simulator**
- Generates realistic sensor data for all supported types
- Configurable detection patterns and noise characteristics
- Responds to L2 control commands

### **Built-in Algorithms** 
- **Target Tracking Algorithm**: Multi-sensor fusion example
- Demonstrates state machine usage and sensor correlation

## Deployment

### **Development Setup**
```bash
# Build system
mkdir build && cd build
cmake .. && make

# Start Redis
redis-server

# Start L2 system
./l2_fusion_system --algorithm TargetTrackingAlgorithm --debug

# Start L1 simulators
./l1_node_simulator --node-id radar_1 --node-type radar --location front
./l1_node_simulator --node-id lidar_1 --node-type lidar --location top
```

### **Production Deployment**
- **Docker Support**: Containerized deployment available
- **Redis Cluster**: High availability configuration
- **Monitoring**: Integration with Prometheus/Grafana
- **Logging**: Structured logging with configurable levels

This architecture provides a solid foundation for aerospace sensor fusion applications while maintaining flexibility for future requirements and algorithm development.