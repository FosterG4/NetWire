cmake_minimum_required(VERSION 3.20)
project(LoggerTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6 configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6 package
find_package(Qt6 REQUIRED COMPONENTS Core)

# Add executable
add_executable(LoggerTest
    test_logger.cpp
    src/globallogger.cpp
    src/globallogger.h
)

# Link libraries
target_link_libraries(LoggerTest PRIVATE
    Qt6::Core
)

# Include directories
target_include_directories(LoggerTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
