cmake_minimum_required(VERSION 3.20)
project(SimpleLoggerTest)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt 6 configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Qt6 package
find_package(Qt6 REQUIRED COMPONENTS Core)

# Add executable
add_executable(SimpleLoggerTest
    test_logger_simple.cpp
    src/globallogger.cpp
    src/globallogger.h
)

# Link libraries
target_link_libraries(SimpleLoggerTest PRIVATE
    Qt6::Core
)

# Include directories
target_include_directories(SimpleLoggerTest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
