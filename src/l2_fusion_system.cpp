#include "l2_fusion_manager.h"
#include "algorithms/target_tracking_algorithm.h"
#include <iostream>
#include <signal.h>
#include <thread>
#include <iomanip>

using namespace dp_aero_l2;

std::atomic<bool> running{true};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down L2 system...\n";
    running = false;
}

void print_usage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --redis-url <url>          Redis connection URL (default: tcp://127.0.0.1:6379)\n";
    std::cout << "  --algorithm <name>         Algorithm to use (default: TargetTrackingAlgorithm)\n";
    std::cout << "  --update-interval <ms>     Algorithm update interval in milliseconds (default: 100)\n";
    std::cout << "  --node-timeout <seconds>   Node timeout in seconds (default: 30)\n";
    std::cout << "  --workers <count>          Number of worker threads (default: 2)\n";
    std::cout << "  --debug                    Enable debug logging\n";
    std::cout << "  --help                     Show this help message\n";
}

core::L2Config parse_arguments(int argc, char* argv[]) {
    core::L2Config config;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help") {
            print_usage(argv[0]);
            exit(0);
        } else if (arg == "--redis-url" && i + 1 < argc) {
            config.redis_connection = argv[++i];
        } else if (arg == "--algorithm" && i + 1 < argc) {
            config.algorithm_name = argv[++i];
        } else if (arg == "--update-interval" && i + 1 < argc) {
            config.algorithm_update_interval = std::chrono::milliseconds(std::stoi(argv[++i]));
        } else if (arg == "--node-timeout" && i + 1 < argc) {
            config.node_timeout = std::chrono::seconds(std::stoi(argv[++i]));
        } else if (arg == "--workers" && i + 1 < argc) {
            config.worker_threads = std::stoi(argv[++i]);
        } else if (arg == "--debug") {
            config.enable_debug_logging = true;
        } else {
            std::cerr << "Unknown argument: " << arg << std::endl;
            print_usage(argv[0]);
            exit(1);
        }
    }
    
    return config;
}

void print_system_info(const core::L2Config& config) {
    std::cout << "=== L2 Fusion System Configuration ===\n";
    std::cout << "Redis URL: " << config.redis_connection << "\n";
    std::cout << "Algorithm: " << config.algorithm_name << "\n";
    std::cout << "Update Interval: " << config.algorithm_update_interval.count() << " ms\n";
    std::cout << "Node Timeout: " << config.node_timeout.count() << " seconds\n";
    std::cout << "Worker Threads: " << config.worker_threads << "\n";
    std::cout << "Debug Logging: " << (config.enable_debug_logging ? "enabled" : "disabled") << "\n";
    std::cout << "=======================================\n\n";
}

void print_stats_periodically(const core::L2FusionManager& manager) {
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        
        if (!running) break;
        
        auto stats = manager.get_stats();
        
        std::cout << "\n=== System Statistics ===\n";
        std::cout << "Uptime: " << stats.uptime.count() << " seconds\n";
        std::cout << "Messages Processed: " << stats.messages_processed << "\n";
        std::cout << "Messages Sent: " << stats.messages_sent << "\n";
        std::cout << "Active Nodes: " << stats.active_nodes << "\n";
        std::cout << "Current State: " << stats.current_algorithm_state << "\n";
        
        if (stats.messages_processed > 0) {
            auto rate = static_cast<double>(stats.messages_processed) / stats.uptime.count();
            std::cout << "Processing Rate: " << std::fixed << std::setprecision(2) << rate << " msg/sec\n";
        }
        
        std::cout << "========================\n\n";
        
        // Print active nodes
        auto& registry = manager.get_node_registry();
        auto active_nodes = registry.get_active_nodes(std::chrono::seconds(30));
        
        if (!active_nodes.empty()) {
            std::cout << "Active L1 Nodes:\n";
            for (const auto& node_id : active_nodes) {
                auto node = registry.get_node(node_id);
                if (node) {
                    std::cout << "  - " << node_id << " (" << node->node_type() << ")\n";
                }
            }
            std::cout << "\n";
        }
    }
}

int main(int argc, char* argv[]) {
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        // Parse command line arguments
        auto config = parse_arguments(argc, argv);
        
        // Print configuration
        print_system_info(config);
        
        // Create L2 fusion manager
        core::L2FusionManager fusion_manager(config);
        
        // Create and register algorithms
        fusion::AlgorithmRegistry registry;
        registry.register_algorithm<algorithms::TargetTrackingAlgorithm>();
        
        // Set the algorithm
        auto algorithm = registry.create_algorithm(config.algorithm_name);
        if (!algorithm) {
            std::cerr << "Error: Unknown algorithm '" << config.algorithm_name << "'\n";
            std::cout << "Available algorithms:\n";
            for (const auto& name : registry.get_available_algorithms()) {
                std::cout << "  - " << name << "\n";
            }
            return 1;
        }
        
        fusion_manager.set_algorithm(std::move(algorithm));
        
        std::cout << "Starting L2 Fusion System...\n";
        std::cout << "Press Ctrl+C to stop\n\n";
        
        // Start the fusion system
        fusion_manager.start();
        
        // Start statistics thread
        std::thread stats_thread(print_stats_periodically, std::cref(fusion_manager));
        
        // Interactive commands
        std::cout << "L2 System is running. Available commands:\n";
        std::cout << "  stats    - Show current statistics\n";
        std::cout << "  nodes    - List active nodes\n";
        std::cout << "  reset    - Reset algorithm state\n";
        std::cout << "  trigger <event> - Trigger algorithm event\n";
        std::cout << "  quit     - Shutdown system\n\n";
        
        std::string input;
        while (running && std::getline(std::cin, input)) {
            if (input == "quit" || input == "exit") {
                running = false;
                break;
            } else if (input == "stats") {
                auto stats = fusion_manager.get_stats();
                std::cout << "Current stats: " << stats.messages_processed << " processed, "
                         << stats.active_nodes << " active nodes, state: " 
                         << stats.current_algorithm_state << "\n";
            } else if (input == "nodes") {
                auto& registry = fusion_manager.get_node_registry();
                auto active_nodes = registry.get_active_nodes(std::chrono::seconds(30));
                std::cout << "Active nodes (" << active_nodes.size() << "):\n";
                for (const auto& node_id : active_nodes) {
                    auto node = registry.get_node(node_id);
                    if (node) {
                        std::cout << "  " << node_id << " (" << node->node_type() 
                                 << ") at " << node->location() << "\n";
                    }
                }
            } else if (input == "reset") {
                fusion_manager.trigger_algorithm_event("reset");
                std::cout << "Algorithm reset triggered\n";
            } else if (input.substr(0, 7) == "trigger") {
                if (input.length() > 8) {
                    std::string event = input.substr(8);
                    fusion_manager.trigger_algorithm_event(event);
                    std::cout << "Triggered event: " << event << "\n";
                } else {
                    std::cout << "Usage: trigger <event_name>\n";
                }
            } else if (!input.empty()) {
                std::cout << "Unknown command. Type 'quit' to exit.\n";
            }
        }
        
        // Clean shutdown
        std::cout << "Shutting down L2 Fusion System...\n";
        fusion_manager.stop();
        
        if (stats_thread.joinable()) {
            stats_thread.join();
        }
        
        std::cout << "L2 Fusion System stopped successfully.\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}