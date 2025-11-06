# DP Aero L2 - Redis Integration

This project demonstrates how to use Redis for inter-process communication with Protocol Buffers in a drone/aerial system context.

## Prerequisites

### Install Redis Server
```bash
# Ubuntu/Debian
sudo apt update
sudo apt install redis-server

# Start Redis
sudo systemctl start redis-server
sudo systemctl enable redis-server
```

### Install Redis++ C++ Client
```bash
# Install dependencies
sudo apt install libhiredis-dev

# Clone and build redis-plus-plus
git clone https://github.com/sewenew/redis-plus-plus.git
cd redis-plus-plus
mkdir build
cd build
cmake ..
make
sudo make install
```

### Install Protocol Buffers
```bash
sudo apt install protobuf-compiler libprotobuf-dev
```

## Build the Project

```bash
mkdir build
cd build
cmake ..
make
```

## Usage Examples

### 1. Redis Pub/Sub Pattern

**Terminal 1 - Start Publisher:**
```bash
./redis_publisher
```

**Terminal 2 - Subscribe to Gimbal Positions:**
```bash
./redis_subscriber pubsub_gimbal
```

**Terminal 3 - Subscribe to Timestamps:**
```bash
./redis_subscriber pubsub_time
```

### 2. Redis Queue Pattern

**Terminal 1 - Start Publisher (also pushes to queue):**
```bash
./redis_publisher
```

**Terminal 2 - Process Queue Messages:**
```bash
./redis_subscriber queue
```

### 3. Redis Streams Pattern

**Terminal 1 - Start Publisher (also adds to stream):**
```bash
./redis_publisher
```

**Terminal 2 - Read Stream Continuously:**
```bash
./redis_stream_example read
```

**Terminal 3 - Analyze Stream Data:**
```bash
./redis_stream_example analytics
```

## Messaging Patterns Explained

### 1. **Pub/Sub (Publish/Subscribe)**
- **Use Case**: Real-time notifications, live telemetry
- **Characteristics**: 
  - Messages are not persisted
  - Multiple subscribers can receive the same message
  - If no subscribers are listening, messages are lost
- **Best For**: Live gimbal position updates, real-time alerts

### 2. **Queues (Lists)**
- **Use Case**: Task distribution, job processing
- **Characteristics**:
  - Messages are persisted until consumed
  - Only one consumer gets each message
  - FIFO (First In, First Out) order
- **Best For**: Command processing, work distribution among multiple processes

### 3. **Streams**
- **Use Case**: Event sourcing, data analytics, replay capability
- **Characteristics**:
  - Messages are persisted with unique IDs
  - Multiple consumers can read the same messages
  - Supports consumer groups for load balancing
  - Can read historical data
- **Best For**: Flight data logging, telemetry history, analytics

## Redis Integration Architecture

```
┌─────────────────┐    Redis Pub/Sub    ┌─────────────────┐
│   Flight        │◄──────────────────►│   Ground        │
│   Controller    │                    │   Station       │
└─────────────────┘                    └─────────────────┘
         │                                       │
         │            Redis Streams              │
         └──────────────────┬────────────────────┘
                           │
                  ┌─────────────────┐
                  │   Data Logger   │
                  │   & Analytics   │
                  └─────────────────┘
```

## Key Features

- **Type-Safe**: Template-based serialization/deserialization
- **Multiple Patterns**: Pub/Sub, Queues, and Streams in one utility
- **Error Handling**: Robust error handling for network issues
- **Flexibility**: Easy to extend for new message types
- **Performance**: Redis in-memory operations for low latency

## Adding New Message Types

1. Define your protobuf message in `proto/` directory
2. Use the `RedisMessenger` class with your new message type:

```cpp
#include "your_message.pb.h"

// Publishing
YourMessage msg;
messenger.publish("your/channel", msg);

// Subscribing
messenger.subscribe<YourMessage>("your/channel", 
    [](const YourMessage& msg) {
        // Handle message
    });
```

## Configuration

Default Redis connection: `tcp://127.0.0.1:6379`

To use a different Redis instance:
```cpp
redis_utils::RedisMessenger messenger("tcp://your-redis-host:6379");
```

For Redis with authentication:
```cpp
redis_utils::RedisMessenger messenger("tcp://user:password@host:6379");
```