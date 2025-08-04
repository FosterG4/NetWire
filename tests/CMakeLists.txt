# Test configuration for NetWire
cmake_minimum_required(VERSION 3.20)

# Enable testing
enable_testing()

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Test)

# Add test executable
add_executable(test_bandwidthchart
    charts/test_bandwidthchart.cpp
)

# Link test executable with required libraries
target_link_libraries(test_bandwidthchart
    PRIVATE
        Qt6::Test
        Qt6::Charts
)

# Include directories
target_include_directories(test_bandwidthchart
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src
)

# Add test to CTest
add_test(NAME test_bandwidthchart COMMAND test_bandwidthchart)
