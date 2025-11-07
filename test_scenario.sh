#!/bin/bash

# L2 Fusion System Test Script
# Tests radar detection -> L2 fusion -> coherent tasking scenario

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}L2 Fusion System Test Scenario${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "${BLUE}Scenario:${NC}"
echo -e "  1. L1 Radar generates target detections every 10 seconds"
echo -e "  2. L2 receives detections, runs fusion and state machine logic"
echo -e "  3. L2 sends commands to L1 Coherent device when targets detected"
echo -e "  4. L1 Coherent prints received commands"
echo ""

# Check if Redis is running
if ! pgrep -x "redis-server" > /dev/null; then
    echo -e "${YELLOW}Warning: Redis server not detected. Starting Redis...${NC}"
    redis-server --daemonize yes --port 6379
    sleep 2
else
    echo -e "${GREEN}✓ Redis server is running${NC}"
fi

# Check if executables exist
if [[ ! -f "./build/l2_fusion_system" ]]; then
    echo -e "${RED}Error: l2_fusion_system executable not found. Please build first.${NC}"
    exit 1
fi

if [[ ! -f "./build/l1_node_simulator" ]]; then
    echo -e "${RED}Error: l1_node_simulator executable not found. Please build first.${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Executables found${NC}"
echo ""

# Function to kill background processes on exit
cleanup() {
    echo -e "\n${YELLOW}Cleaning up processes...${NC}"
    pkill -f "l2_fusion_system"
    pkill -f "l1_node_simulator"
    sleep 1
    echo -e "${GREEN}Cleanup complete${NC}"
}

# Set trap for cleanup
trap cleanup EXIT INT TERM

echo -e "${BLUE}Starting processes...${NC}"
echo ""

# Start L2 Fusion System
echo -e "${YELLOW}[1/3] Starting L2 Fusion System...${NC}"
./build/l2_fusion_system --algorithm TargetTrackingAlgorithm --debug &
L2_PID=$!
sleep 3

# Start L1 Radar Node (generates target detections every 10 seconds)
echo -e "${YELLOW}[2/3] Starting L1 Radar Node...${NC}"
./build/l1_node_simulator \
    --node-id "radar_001" \
    --node-type "radar" \
    --location "front_array" \
    --interval 10000 \
    --detection-prob 0.8 &
RADAR_PID=$!
sleep 2

# Start L1 Coherent Node (listens for L2 commands)
echo -e "${YELLOW}[3/3] Starting L1 Coherent Node...${NC}"
./build/l1_node_simulator \
    --node-id "coherent_001" \
    --node-type "coherent" \
    --location "beam_director" \
    --interval 30000 \
    --detection-prob 0.1 &
COHERENT_PID=$!
sleep 2

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}All processes started successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "${BLUE}Process Status:${NC}"
echo -e "  L2 Fusion System: PID $L2_PID"
echo -e "  L1 Radar Node:    PID $RADAR_PID"  
echo -e "  L1 Coherent Node: PID $COHERENT_PID"
echo ""
echo -e "${YELLOW}Expected Behavior:${NC}"
echo -e "  • Radar generates detections every 10 seconds"
echo -e "  • L2 processes detections and transitions through states:"
echo -e "    IDLE → ACQUIRING → TRACKING"
echo -e "  • When in TRACKING state, L2 sends gimbal commands to Coherent"
echo -e "  • Coherent device prints detailed command information"
echo ""
echo -e "${GREEN}Press Ctrl+C to stop all processes${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""

# Monitor processes and show their status
while true; do
    # Check if any process died
    if ! kill -0 $L2_PID 2>/dev/null; then
        echo -e "${RED}L2 Fusion System died unexpectedly${NC}"
        break
    fi
    
    if ! kill -0 $RADAR_PID 2>/dev/null; then
        echo -e "${RED}L1 Radar Node died unexpectedly${NC}"
        break
    fi
    
    if ! kill -0 $COHERENT_PID 2>/dev/null; then
        echo -e "${RED}L1 Coherent Node died unexpectedly${NC}"
        break
    fi
    
    sleep 5
done