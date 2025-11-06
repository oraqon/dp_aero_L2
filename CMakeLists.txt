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

# Use installed redis++ library
find_library(REDIS_PLUS_PLUS_LIB redis++ PATHS /usr/local/lib)
if(NOT REDIS_PLUS_PLUS_LIB)
    message(FATAL_ERROR "redis++ library not found")
endif()

set(REDIS_PLUS_PLUS_INCLUDE_DIRS /usr/local/include)
set(REDIS_PLUS_PLUS_LIBRARIES ${REDIS_PLUS_PLUS_LIB})

# Include directories
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Protobuf files
file(GLOB_RECURSE PROTO_FILES "proto/*.proto")

# Generate C++ files from protobuf with proper import paths
set(PROTO_SRCS "")
set(PROTO_HDRS "")

foreach(PROTO_FILE ${PROTO_FILES})
    get_filename_component(PROTO_NAME ${PROTO_FILE} NAME_WE)
    get_filename_component(PROTO_PATH ${PROTO_FILE} DIRECTORY)
    file(RELATIVE_PATH PROTO_REL_PATH ${CMAKE_CURRENT_SOURCE_DIR}/proto ${PROTO_PATH})
    
    set(PROTO_SRC "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_REL_PATH}/${PROTO_NAME}.pb.cc")
    set(PROTO_HDR "${CMAKE_CURRENT_BINARY_DIR}/${PROTO_REL_PATH}/${PROTO_NAME}.pb.h")
    
    list(APPEND PROTO_SRCS ${PROTO_SRC})
    list(APPEND PROTO_HDRS ${PROTO_HDR})
    
    add_custom_command(
        OUTPUT ${PROTO_SRC} ${PROTO_HDR}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/${PROTO_REL_PATH}
        COMMAND ${Protobuf_PROTOC_EXECUTABLE}
        ARGS --cpp_out ${CMAKE_CURRENT_BINARY_DIR} -I ${CMAKE_CURRENT_SOURCE_DIR}/proto ${PROTO_FILE}
        DEPENDS ${PROTO_FILE}
        COMMENT "Running C++ protocol buffer compiler on ${PROTO_FILE}"
        VERBATIM
    )
endforeach()

# Create library
add_library(dp_aero_l2_proto STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(dp_aero_l2_proto ${Protobuf_LIBRARIES})
target_include_directories(dp_aero_l2_proto PUBLIC ${Protobuf_INCLUDE_DIRS})

# Note: dp_aero_l2_example removed - source files don't exist

# Examples temporarily disabled for initial build
# Redis Publisher example
# add_executable(redis_publisher 
#     src/redis_publisher.cpp
# )

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

# Compiler flags for protobuf library
target_compile_options(dp_aero_l2_proto PRIVATE -Wall -Wextra -O2)

# Install targets
install(TARGETS dp_aero_l2_proto
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)