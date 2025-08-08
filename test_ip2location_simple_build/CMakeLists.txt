cmake_minimum_required(VERSION 3.20)
project(TestIP2LocationSimple VERSION 0.1.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6 configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6 package with required components
find_package(Qt6 REQUIRED COMPONENTS 
    Core 
    Network
    Widgets
)

# Create test executable
add_executable(TestIP2LocationSimple
    test_ip2location_simple.cpp
    src/ip2location.cpp
)

# Link libraries
target_link_libraries(TestIP2LocationSimple PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Widgets
)

# Include directories
target_include_directories(TestIP2LocationSimple PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set output directory
set_target_properties(TestIP2LocationSimple PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)
