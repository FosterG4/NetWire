cmake_minimum_required(VERSION 3.20)
project(NetWire VERSION 0.1.0 LANGUAGES CXX)

# C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Find Vulkan headers to satisfy Qt6 requirements
find_package(VulkanHeaders CONFIG QUIET)
if(VulkanHeaders_FOUND)
    message(STATUS "Found Vulkan headers via vcpkg")
else()
    message(STATUS "Vulkan headers not found, but this is optional")
endif()

# Find required packages
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Core Gui Charts)

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

# Add source files
add_executable(NetWire
    src/main.cpp
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
    src/utils/animations.cpp
    # Headers
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
    src/utils/animations.h
    # UI files
    src/firewallrulesdialog.ui
    src/alertsdialog.ui
    src/alertsettingsdialog.ui
    ${RESOURCES}
)

# Link libraries
target_link_libraries(NetWire PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Core
    Qt6::Gui
    Qt6::Charts
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

# Install rules
install(TARGETS NetWire
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)
