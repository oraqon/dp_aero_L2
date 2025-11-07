#!/bin/bash

# Individual component launcher for L2 Fusion System testing

show_help() {
    echo "L2 Fusion System Component Launcher"
    echo ""
    echo "Usage: $0 [COMPONENT]"
    echo ""
    echo "Components:"
    echo "  l2         - Start L2 Fusion System"
    echo "  radar      - Start L1 Radar Node (generates detections every 10s)"
    echo "  coherent   - Start L1 Coherent Node (listens for commands)"
    echo "  all        - Start all components (same as test_scenario.sh)"
    echo "  help       - Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0 l2                    # Start only L2 system"
    echo "  $0 radar                 # Start only radar node" 
    echo "  $0 coherent              # Start only coherent node"
    echo ""
    echo "For full test scenario, run: ./test_scenario.sh"
}

case "$1" in
    "l2")
        echo "Starting L2 Fusion System..."
        ./build/l2_fusion_system --algorithm TargetTrackingAlgorithm --debug
        ;;
    "radar")
        echo "Starting L1 Radar Node (detections every 10 seconds)..."
        ./build/l1_node_simulator \
            --node-id "radar_001" \
            --node-type "radar" \
            --location "front_array" \
            --interval 10000 \
            --detection-prob 0.8
        ;;
    "coherent")
        echo "Starting L1 Coherent Node..."
        ./build/l1_node_simulator \
            --node-id "coherent_001" \
            --node-type "coherent" \
            --location "beam_director" \
            --interval 30000 \
            --detection-prob 0.1
        ;;
    "all")
        echo "Starting full test scenario..."
        ./test_scenario.sh
        ;;
    "help"|"-h"|"--help"|"")
        show_help
        ;;
    *)
        echo "Error: Unknown component '$1'"
        echo ""
        show_help
        exit 1
        ;;
esac