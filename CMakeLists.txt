cmake_minimum_required(VERSION 3.20)
project(NetWire VERSION 0.1.0 LANGUAGES CXX)

# Set Qt6 installation path
set(QT_ROOT "C:/Qt/6.9.1/msvc2022_64" CACHE PATH "Path to Qt6 installation")

# Print debug information
message(STATUS "Current working directory: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "QT_ROOT: ${QT_ROOT}")

# Verify Qt6 directory exists
if(NOT EXISTS "${QT_ROOT}")
    message(FATAL_ERROR "Qt6 directory not found at: ${QT_ROOT}")
endif()

# Set the path to the Qt6Config.cmake file
set(Qt6_DIR "${QT_ROOT}/lib/cmake/Qt6" CACHE PATH "Path to Qt6 CMake configuration" FORCE)
set(CMAKE_PREFIX_PATH "${QT_ROOT}" CACHE STRING "" FORCE)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Windows-specific settings
if(WIN32)
    set(CMAKE_WIN32_EXECUTABLE TRUE)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
endif()

# Find Qt6 package with required components
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Charts Concurrent)

# Print debug info
message(STATUS "Found Qt6 version: ${Qt6_VERSION}")
message(STATUS "Qt6 Core found: ${Qt6Core_FOUND}")
message(STATUS "Qt6 Gui found: ${Qt6Gui_FOUND}")
message(STATUS "Qt6 Widgets found: ${Qt6Widgets_FOUND}")
message(STATUS "Qt6 Network found: ${Qt6Network_FOUND}")
message(STATUS "Qt6 Charts found: ${Qt6Charts_FOUND}")
message(STATUS "Qt6 Concurrent found: ${Qt6Concurrent_FOUND}")

# Set include directories
include_directories(${Qt6Core_INCLUDE_DIRS})
include_directories(${Qt6Gui_INCLUDE_DIRS})
include_directories(${Qt6Widgets_INCLUDE_DIRS})
include_directories(${Qt6Network_INCLUDE_DIRS})
include_directories(${Qt6Charts_INCLUDE_DIRS})
include_directories(${Qt6Concurrent_INCLUDE_DIRS})

# Find Vulkan headers to satisfy Qt6 requirements
find_package(VulkanHeaders CONFIG QUIET)
if(VulkanHeaders_FOUND)
    message(STATUS "Found Vulkan headers via vcpkg")
else()
    message(STATUS "Vulkan headers not found, but this is optional")
endif()

# Find required Qt6 components
set(QT_COMPONENTS
    Core
    Gui
    Widgets
    Network
    Charts
    Concurrent
)

# Find Qt6 package with all required components
find_package(Qt6 REQUIRED COMPONENTS ${QT_COMPONENTS})

# Set include directories
include_directories(${Qt6Core_INCLUDE_DIRS})
include_directories(${Qt6Gui_INCLUDE_DIRS})
include_directories(${Qt6Widgets_INCLUDE_DIRS})
include_directories(${Qt6Network_INCLUDE_DIRS})
include_directories(${Qt6Charts_INCLUDE_DIRS})

# libpcap configuration for Windows
# vcpkg toolchain will be set via command line

# Try to find libpcap package using vcpkg
find_package(libpcap CONFIG QUIET)

if(libpcap_FOUND)
    # Set up libpcap variables
    set(PCAP_INCLUDE_DIRS "")
    set(PCAP_LIBRARIES libpcap::libpcap)
    set(PCAP_FOUND TRUE)
    add_definitions(-DHAVE_PCAP)
    message(STATUS "Found libpcap via vcpkg: ${PCAP_LIBRARIES}")
else()
    # Fallback to traditional find_package
    find_package(libpcap QUIET)
    if(libpcap_FOUND)
        set(PCAP_INCLUDE_DIRS "")
        set(PCAP_LIBRARIES libpcap)
        set(PCAP_FOUND TRUE)
        add_definitions(-DHAVE_PCAP)
        message(STATUS "Found libpcap via traditional find_package: ${PCAP_LIBRARIES}")
    else()
        # Manual fallback for vcpkg installed libpcap
        set(PCAP_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/include")
        set(PCAP_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/lib/pcap.lib")
        if(EXISTS ${PCAP_LIBRARIES})
            set(PCAP_FOUND TRUE)
            add_definitions(-DHAVE_PCAP)
            message(STATUS "Found libpcap manually: ${PCAP_LIBRARIES}")
        else()
            message(WARNING "libpcap not found. Network monitoring features will be disabled.")
            # Set empty variables to avoid build errors
            set(PCAP_INCLUDE_DIRS "")
            set(PCAP_LIBRARIES "")
            set(PCAP_FOUND FALSE)
        endif()
    endif()
endif()

# Set up resources
set(RESOURCES
    resources.qrc
)

# Add minimal resource file for testing
qt6_add_resources(RESOURCE_FILES
    minimal.qrc
)

# Add source files
add_executable(NetWire
    src/main.cpp
    src/globallogger.cpp
    src/mainwindow.cpp
    src/networkmonitor.cpp
    src/firewallmanager.cpp
    src/firewallrulesdialog.cpp
    src/alertmanager.cpp
    src/alertsdialog.cpp
    src/alertsettingsdialog.cpp
    # Dashboard and Charts components
    src/dashboard/dashboardwidget.cpp
    src/dashboard/networkcharts.cpp
    src/charts/bandwidthchart.cpp
    src/charts/connectionchart.cpp
    src/charts/applicationpiechart.cpp
    src/charts/connectiontimelinechart.cpp
    src/charts/networkheatmap.cpp
    src/utils/animations.cpp
    # Headers
    src/globallogger.h
    src/networkmonitor.h
    src/mainwindow.h
    src/firewallmanager.h
    src/firewallrulesdialog.h
    src/alertmanager.h
    src/alertsdialog.h
    src/alertsettingsdialog.h
    src/dashboard/dashboardwidget.h
    src/dashboard/networkcharts.h
    src/charts/bandwidthchart.h
    src/charts/connectionchart.h
    src/charts/applicationpiechart.h
    src/charts/connectiontimelinechart.h
    src/charts/networkheatmap.h
    src/utils/animations.h
    # UI files
    src/firewallrulesdialog.ui
    src/alertsdialog.ui
    src/alertsettingsdialog.ui
    ${RESOURCE_FILES}
)

# Link libraries
target_link_libraries(NetWire PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Core
    Qt6::Gui
    Qt6::Charts
    Qt6::Concurrent
)

# Only link pcap libraries if found
if(PCAP_FOUND)
    target_link_libraries(NetWire PRIVATE ${PCAP_LIBRARIES})
endif()

# Only include pcap directories if found
if(PCAP_FOUND)
    target_include_directories(NetWire PRIVATE ${PCAP_INCLUDE_DIRS})
endif()

# Include directories
target_include_directories(NetWire PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${PCAP_INCLUDE_DIRS}
)

# Add Qt Charts include directory
target_include_directories(NetWire PRIVATE
    ${Qt6Charts_INCLUDE_DIRS}
    ${Qt6Charts_INCLUDE_DIRS}/QtCharts
)

# Windows-specific target properties
if(WIN32)
    set_target_properties(NetWire PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    # Add definition to use only one entry point
    target_compile_definitions(NetWire PRIVATE WIN32_LEAN_AND_MEAN)
    
    # Set manifest for admin privileges
    set_target_properties(NetWire PROPERTIES
        LINK_FLAGS "/MANIFEST:NO"
    )
    
    # Add manifest file for admin privileges
    target_sources(NetWire PRIVATE app.manifest)
endif()

# Install rules
install(TARGETS NetWire
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)
