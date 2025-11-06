cmake_minimum_required(VERSION 3.20)
project(dp_aero_L2 VERSION 1.0.0 LANGUAGES CXX)

# Set C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(Protobuf REQUIRED)
find_package(PkgConfig REQUIRED)

# Find Redis++ (hiredis-based C++ client)
find_package(PkgConfig REQUIRED)
pkg_check_modules(HIREDIS REQUIRED hiredis)
pkg_check_modules(REDIS_PLUS_PLUS REQUIRED redis++)

# Alternative: if using vcpkg or manual installation
# find_package(hiredis REQUIRED)
# find_package(redis++ REQUIRED)

# Include directories
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Protobuf files
file(GLOB_RECURSE PROTO_FILES "proto/*.proto")

# Generate C++ files from protobuf
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Create library
add_library(dp_aero_l2_proto STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(dp_aero_l2_proto ${Protobuf_LIBRARIES})
target_include_directories(dp_aero_l2_proto PUBLIC ${Protobuf_INCLUDE_DIRS})

# Example executable
add_executable(dp_aero_l2_example 
    src/main.cpp
    src/data_processor.cpp
)

target_link_libraries(dp_aero_l2_example 
    dp_aero_l2_proto
    ${Protobuf_LIBRARIES}
    ${HIREDIS_LIBRARIES}
    ${REDIS_PLUS_PLUS_LIBRARIES}
)

target_include_directories(dp_aero_l2_example PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
    ${REDIS_PLUS_PLUS_INCLUDE_DIRS}
)

# Redis Publisher example
add_executable(redis_publisher 
    src/redis_publisher.cpp
)

target_link_libraries(redis_publisher 
    dp_aero_l2_proto
    ${Protobuf_LIBRARIES}
    ${HIREDIS_LIBRARIES}
    ${REDIS_PLUS_PLUS_LIBRARIES}
)

target_include_directories(redis_publisher PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
    ${REDIS_PLUS_PLUS_INCLUDE_DIRS}
)

# Redis Subscriber example
add_executable(redis_subscriber 
    src/redis_subscriber.cpp
)

target_link_libraries(redis_subscriber 
    dp_aero_l2_proto
    ${Protobuf_LIBRARIES}
    ${HIREDIS_LIBRARIES}
    ${REDIS_PLUS_PLUS_LIBRARIES}
)

target_include_directories(redis_subscriber PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
    ${REDIS_PLUS_PLUS_INCLUDE_DIRS}
)

# Redis Stream example
add_executable(redis_stream_example 
    src/redis_stream_example.cpp
)

target_link_libraries(redis_stream_example 
    dp_aero_l2_proto
    ${Protobuf_LIBRARIES}
    ${HIREDIS_LIBRARIES}
    ${REDIS_PLUS_PLUS_LIBRARIES}
)

target_include_directories(redis_stream_example PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
    ${REDIS_PLUS_PLUS_INCLUDE_DIRS}
)

# L2 Fusion System
add_executable(l2_fusion_system 
    src/l2_fusion_system.cpp
    src/algorithm_framework.cpp
)

target_link_libraries(l2_fusion_system 
    dp_aero_l2_proto
    ${Protobuf_LIBRARIES}
    ${HIREDIS_LIBRARIES}
    ${REDIS_PLUS_PLUS_LIBRARIES}
)

target_include_directories(l2_fusion_system PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
    ${REDIS_PLUS_PLUS_INCLUDE_DIRS}
)

# L1 Node Simulator
add_executable(l1_node_simulator 
    src/l1_node_simulator.cpp
)

target_link_libraries(l1_node_simulator 
    dp_aero_l2_proto
    ${Protobuf_LIBRARIES}
    ${HIREDIS_LIBRARIES}
    ${REDIS_PLUS_PLUS_LIBRARIES}
)

target_include_directories(l1_node_simulator PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
    ${HIREDIS_INCLUDE_DIRS}
    ${REDIS_PLUS_PLUS_INCLUDE_DIRS}
)

# Compiler flags for all targets
target_compile_options(dp_aero_l2_example PRIVATE -Wall -Wextra -O2)
target_compile_options(redis_publisher PRIVATE -Wall -Wextra -O2)
target_compile_options(redis_subscriber PRIVATE -Wall -Wextra -O2)
target_compile_options(redis_stream_example PRIVATE -Wall -Wextra -O2)
target_compile_options(l2_fusion_system PRIVATE -Wall -Wextra -O2 -pthread)
target_compile_options(l1_node_simulator PRIVATE -Wall -Wextra -O2 -pthread)

# Install targets
install(TARGETS dp_aero_l2_proto dp_aero_l2_example redis_publisher redis_subscriber redis_stream_example l2_fusion_system l1_node_simulator
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)