cmake_minimum_required(VERSION 3.20)
project(TestIP2Location VERSION 0.1.0 LANGUAGES CXX)

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
)

# Create test executable
add_executable(TestIP2Location
    test_ip2location.cpp
    src/ip2location.cpp
)

# Link libraries
target_link_libraries(TestIP2Location PRIVATE
    Qt6::Core
    Qt6::Network
)

# Include directories
target_include_directories(TestIP2Location PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# Set output directory
set_target_properties(TestIP2Location PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests
)
