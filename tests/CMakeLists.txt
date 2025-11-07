cmake_minimum_required(VERSION 3.20)

# Google Test setup
if(TARGET gtest)
    set(GTEST_LIBRARIES gtest gtest_main)
    set(GTEST_INCLUDE_DIRS "")
else()
    find_package(GTest REQUIRED)
    set(GTEST_LIBRARIES GTest::gtest GTest::gtest_main)
    set(GTEST_INCLUDE_DIRS "")
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_BINARY_DIR})  # For protobuf headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/fixtures)

# Common test libraries (sources that tests depend on)
set(TEST_COMMON_SOURCES
    ${CMAKE_SOURCE_DIR}/src/algorithm_strategies.cpp
    ${CMAKE_SOURCE_DIR}/src/task_manager.cpp
    ${CMAKE_SOURCE_DIR}/src/algorithm_framework.cpp
)

# Strategy tests (highest priority)
add_executable(test_strategies
    unit/strategies/test_confidence_prioritizer.cpp
    unit/strategies/test_threat_prioritizer.cpp
    ${TEST_COMMON_SOURCES}
)

target_link_libraries(test_strategies
    ${GTEST_LIBRARIES}
    dp_aero_l2_proto
    ${Protobuf_LIBRARIES}
    ${HIREDIS_LIBRARIES}
    ${REDIS_PLUS_PLUS_LIBRARIES}
    pthread
)

# Framework tests
add_executable(test_framework
    unit/framework/test_algorithm_context_simple.cpp
    unit/framework/test_task_manager_clean.cpp
    ${TEST_COMMON_SOURCES}
)

target_link_libraries(test_framework
    ${GTEST_LIBRARIES}
    dp_aero_l2_proto
    ${Protobuf_LIBRARIES}
    ${HIREDIS_LIBRARIES}
    ${REDIS_PLUS_PLUS_LIBRARIES}
    pthread
)

# Register tests with CTest
add_test(NAME StrategyTests COMMAND test_strategies)
add_test(NAME FrameworkTests COMMAND test_framework)

# Set test properties
set_tests_properties(StrategyTests PROPERTIES 
    TIMEOUT 30
    LABELS "unit;strategies"
)

set_tests_properties(FrameworkTests PROPERTIES 
    TIMEOUT 30
    LABELS "unit;framework"
)