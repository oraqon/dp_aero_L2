# Test Scenario: Radar → L2 Fusion → Coherent Device

This test demonstrates the complete flow from radar detection to coherent device tasking through the L2 fusion system.

## Scenario Overview

```
┌─────────────┐    Target        ┌─────────────┐    Gimbal        ┌─────────────┐
│   L1 Radar  │   Detections     │ L2 Fusion   │    Commands      │ L1 Coherent │
│   Node      │ ───────────────► │  System     │ ───────────────► │   Device    │
│             │   (Every 10s)    │             │   (When target   │             │
│ radar_001   │                  │             │    detected)     │ coherent_001│
└─────────────┘                  └─────────────┘                  └─────────────┘
```

## Components

### 1. **L1 Radar Node** (`radar_001`)
- **Function**: Generates target detections every 10 seconds
- **Detection Rate**: 80% probability of detecting targets
- **Output**: Radar detection messages sent to L2 via Redis

### 2. **L2 Fusion System**
- **Function**: Processes radar detections using TargetTrackingAlgorithm
- **State Machine**: `IDLE → ACQUIRING → TRACKING → LOST`
- **Task Manager**: Creates tracking tasks and assigns to devices
- **Output**: Gimbal commands sent to coherent device when tracking

### 3. **L1 Coherent Device** (`coherent_001`)  
- **Function**: Receives and processes L2 commands
- **Response**: Prints detailed command information when tasked
- **Capability**: Beam pointing and target engagement

## Expected Flow

1. **Radar Detection** (every 10s)
   - Radar generates target detections with position data
   - Sends `L1ToL2Message` with radar data to L2

2. **L2 Processing**
   - Receives radar message in `process_l1_message()`
   - Creates new target and tracking task
   - Assigns task to default device via TaskManager
   - State machine transitions: `IDLE → ACQUIRING → TRACKING`

3. **Coherent Tasking** (when in TRACKING state)
   - Calculates target gimbal angles (azimuth/elevation)
   - Sends `POINT_GIMBAL` command to `coherent_001`
   - Command includes target coordinates

4. **Coherent Response**
   - Coherent device receives targeted message
   - Prints detailed engagement information
   - Shows beam pointing angles and engagement status

## Running the Test

### Option 1: Full Automated Test
```bash
./test_scenario.sh
```

### Option 2: Manual Component Control
```bash
# Terminal 1: Start L2 system
./run_component.sh l2

# Terminal 2: Start radar
./run_component.sh radar  

# Terminal 3: Start coherent device
./run_component.sh coherent
```

### Option 3: Individual Components
```bash
# L2 Fusion System
./build/l2_fusion_system --algorithm TargetTrackingAlgorithm --debug

# L1 Radar (10 second intervals, 80% detection probability)
./build/l1_node_simulator --node-id radar_001 --node-type radar --location front_array --interval 10000 --detection-prob 0.8

# L1 Coherent Device  
./build/l1_node_simulator --node-id coherent_001 --node-type coherent --location beam_director --interval 30000 --detection-prob 0.1
```

## Expected Output

### L1 Radar Node
```
[radar_001] Sent radar data
[radar_001] Heartbeat sent
```

### L2 Fusion System  
```
[TargetTrackingAlgorithm] INFO: Created tracking task task_1 for new target target_0
[TargetTrackingAlgorithm] INFO: Entered ACQUIRING state
[TargetTrackingAlgorithm] INFO: Entered TRACKING state
[TargetTrackingAlgorithm] INFO: *** TASKING COHERENT DEVICE ***
[TargetTrackingAlgorithm] INFO: Sent gimbal command to coherent_001 for target target_0 (theta: 0.123, phi: -0.045)
```

### L1 Coherent Device
```
[coherent_001] Received L2 message: Control Command - POINT_GIMBAL - theta: 0.123, phi: -0.045
[coherent_001] *** COHERENT DEVICE TASKED ***
[coherent_001] Pointing coherent beam to target coordinates
[coherent_001] Azimuth: 7.05 degrees
[coherent_001] Elevation: -2.58 degrees  
[coherent_001] Coherent beam engagement initiated!
```

## Verification Points

- ✅ Radar generates detections every 10 seconds
- ✅ L2 receives radar messages and processes them
- ✅ TaskManager creates tasks and assigns to devices
- ✅ State machine transitions through states correctly
- ✅ L2 sends targeted commands to specific coherent device
- ✅ Coherent device receives and processes commands
- ✅ End-to-end message flow via Redis pub/sub

## Configuration

### Timing
- **Radar interval**: 10 seconds between detections
- **Coherent status**: 30 seconds between status updates  
- **L2 algorithm update**: 50ms (default)

### Detection Parameters
- **Radar detection probability**: 80%
- **Target confidence thresholds**: Configurable in algorithm
- **State transition criteria**: Based on sensor consensus

This test validates the complete target detection and engagement workflow from sensor to effector through the L2 fusion system with TaskManager coordination.