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

# C++ Standard - use C++20 for modern features
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Ensure we use MSVC properly
if(MSVC)
    # Use default C++ flags for MSVC
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHsc")
    # Enable C++20 features
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++20")
    # Allow concurrent cl.exe processes to share PDB safely
    add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/FS>)
    # Ensure PDB format is ProgramDatabase for multi-proc builds
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT ProgramDatabase)
endif()

# Windows-specific settings
if(WIN32)
    set(CMAKE_WIN32_EXECUTABLE TRUE)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    add_definitions(-DNOMINMAX)
endif()

# Locate Vulkan headers (WrapVulkanHeaders) if available so Qt can enable related features
if(WIN32 AND NOT DEFINED Vulkan_INCLUDE_DIR)
    if(DEFINED ENV{VULKAN_SDK} AND EXISTS "$ENV{VULKAN_SDK}/Include")
        set(Vulkan_INCLUDE_DIR "$ENV{VULKAN_SDK}/Include" CACHE PATH "Path to Vulkan headers from VULKAN_SDK" FORCE)
        message(STATUS "Using Vulkan headers from VULKAN_SDK: ${Vulkan_INCLUDE_DIR}")
    elseif(EXISTS "C:/VulkanSDK")
        file(GLOB _VULKAN_SDK_DIRS "C:/VulkanSDK/*")
        list(SORT _VULKAN_SDK_DIRS DESC)
        foreach(_dir ${_VULKAN_SDK_DIRS})
            if(EXISTS "${_dir}/Include")
                set(Vulkan_INCLUDE_DIR "${_dir}/Include" CACHE PATH "Path to Vulkan headers" FORCE)
                message(STATUS "Detected Vulkan SDK at: ${_dir}")
                break()
            endif()
        endforeach()
    endif()
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

# Optionally detect VulkanHeaders package if available (quiet)
find_package(VulkanHeaders CONFIG QUIET)
if(VulkanHeaders_FOUND)
    message(STATUS "Found VulkanHeaders package")
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
# Try multiple methods to find packet capture library

# Method 1: Try to find libpcap package using vcpkg
find_package(libpcap CONFIG QUIET)

if(libpcap_FOUND)
    # Set up libpcap variables
    set(PCAP_INCLUDE_DIRS "")
    set(PCAP_LIBRARIES libpcap::libpcap)
    set(PCAP_FOUND TRUE)
    add_definitions(-DHAVE_PCAP)
    message(STATUS "Found libpcap via vcpkg: ${PCAP_LIBRARIES}")
else()
    # Method 2: Try to find Npcap (Windows packet capture library)
    find_path(NPCAP_INCLUDE_DIR pcap.h
        PATHS
        "C:/Program Files/Npcap/SDK/Include"
        "C:/Program Files (x86)/Npcap/SDK/Include"
        "C:/Npcap/SDK/Include"
        "C:/Program Files/WpdPack/Include"
        "C:/Program Files (x86)/WpdPack/Include"
        DOC "Npcap include directory"
    )
    
    find_library(NPCAP_LIBRARY
        NAMES wpcap pcap
        PATHS
        "C:/Program Files/Npcap/SDK/Lib/x64"
        "C:/Program Files (x86)/Npcap/SDK/Lib/x64"
        "C:/Npcap/SDK/Lib/x64"
        "C:/Program Files/WpdPack/Lib/x64"
        "C:/Program Files (x86)/WpdPack/Lib/x64"
        DOC "Npcap library"
    )
    
    if(NPCAP_INCLUDE_DIR AND NPCAP_LIBRARY)
        set(PCAP_INCLUDE_DIRS "${NPCAP_INCLUDE_DIR}")
        set(PCAP_LIBRARIES "${NPCAP_LIBRARY}")
        set(PCAP_FOUND TRUE)
        add_definitions(-DHAVE_PCAP)
        message(STATUS "Found Npcap: ${PCAP_LIBRARIES}")
    else()
        # Method 3: Fallback to traditional find_package
        find_package(libpcap QUIET)
        if(libpcap_FOUND)
            set(PCAP_INCLUDE_DIRS "")
            set(PCAP_LIBRARIES libpcap)
            set(PCAP_FOUND TRUE)
            add_definitions(-DHAVE_PCAP)
            message(STATUS "Found libpcap via traditional find_package: ${PCAP_LIBRARIES}")
        else()
            # Method 4: Manual fallback for vcpkg installed libpcap
            set(PCAP_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/include")
            set(PCAP_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/lib/pcap.lib")
            if(EXISTS ${PCAP_LIBRARIES})
                set(PCAP_FOUND TRUE)
                add_definitions(-DHAVE_PCAP)
                message(STATUS "Found libpcap manually: ${PCAP_LIBRARIES}")
            else()
                # Method 5: Try to find WinPcap (older Windows packet capture)
                find_path(WINPCAP_INCLUDE_DIR pcap.h
                    PATHS
                    "C:/Program Files/WinPcap/Include"
                    "C:/Program Files (x86)/WinPcap/Include"
                    DOC "WinPcap include directory"
                )
                
                find_library(WINPCAP_LIBRARY
                    NAMES wpcap
                    PATHS
                    "C:/Program Files/WinPcap/Lib"
                    "C:/Program Files (x86)/WinPcap/Lib"
                    DOC "WinPcap library"
                )
                
                if(WINPCAP_INCLUDE_DIR AND WINPCAP_LIBRARY)
                    set(PCAP_INCLUDE_DIRS "${WINPCAP_INCLUDE_DIR}")
                    set(PCAP_LIBRARIES "${WINPCAP_LIBRARY}")
                    set(PCAP_FOUND TRUE)
                    add_definitions(-DHAVE_PCAP)
                    message(STATUS "Found WinPcap: ${PCAP_LIBRARIES}")
                else()
                    message(WARNING "No packet capture library found (libpcap/Npcap/WinPcap).")
                    message(WARNING "Network monitoring features will be disabled.")
                    message(WARNING "To enable network monitoring, install Npcap from: https://npcap.com/")
                    # Set empty variables to avoid build errors
                    set(PCAP_INCLUDE_DIRS "")
                    set(PCAP_LIBRARIES "")
                    set(PCAP_FOUND FALSE)
                endif()
            endif()
        endif()
    endif()
endif()

# Set up resources (compile both primary and minimal resource collections)
qt6_add_resources(RESOURCE_FILES
    resources.qrc
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
    src/intrusiondetectionmanager.cpp
    # src/connectiondetailsdialog.cpp # Temporarily disabled due to QtCharts issues
    src/iplookup.cpp
    src/ip2location.cpp
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
    # src/connectiondetailsdialog.h # Temporarily disabled due to QtCharts issues
    src/iplookup.h
    src/ip2location.h
    src/dashboard/dashboardwidget.h
    src/dashboard/networkcharts.h
    src/charts/bandwidthchart.h
    src/charts/connectionchart.h
    src/charts/applicationpiechart.h
    src/charts/connectiontimelinechart.h
    src/charts/networkheatmap.h
    src/utils/animations.h
    # UI files
    src/mainwindow.ui
    src/firewallrulesdialog.ui
    src/alertsdialog.ui
    src/alertsettingsdialog.ui
    # src/connectiondetailsdialog.ui # Temporarily disabled due to QtCharts issues
    ${RESOURCE_FILES}
)

# Add Windows manifest for administrator privileges
if(WIN32)
    # Create a resource file for the manifest
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/NetWire.manifest 
                   ${CMAKE_CURRENT_BINARY_DIR}/NetWire.manifest COPYONLY)
    
    # Add manifest as a resource
    set_target_properties(NetWire PROPERTIES
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\""
    )
endif()

# Link libraries
target_link_libraries(NetWire PRIVATE
    Qt6::Widgets
    Qt6::Network
    Qt6::Core
    Qt6::Gui
    Qt6::Concurrent
)

# Link Charts
target_link_libraries(NetWire PRIVATE Qt6::Charts)

# Only link pcap libraries if found
if(PCAP_FOUND)
    target_link_libraries(NetWire PRIVATE ${PCAP_LIBRARIES})
endif()

# Only include pcap directories if found
if(PCAP_FOUND)
    target_include_directories(NetWire PRIVATE ${PCAP_INCLUDE_DIRS})
    message(STATUS "Network monitoring enabled with packet capture library")
else()
    message(STATUS "Network monitoring disabled - no packet capture library found")
    message(STATUS "To enable network monitoring:")
    message(STATUS "  1. Download Npcap from: https://npcap.com/")
    message(STATUS "  2. Install with SDK option enabled")
    message(STATUS "  3. Rebuild the project")
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
    "C:/Qt/6.9.1/msvc2022_64/include/QtCharts"
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
    
    # Add manifest file for admin privileges (if exists)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app.manifest")
        target_sources(NetWire PRIVATE app.manifest)
    endif()
endif()

# Install rules
install(TARGETS NetWire
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION .
)

# ----------------------
# Windows runtime deploy
# ----------------------
if(WIN32)
    # Create a deploy directory next to the build dir and collect all runtime dependencies
    set(DEPLOY_DIR "${CMAKE_BINARY_DIR}/deploy")

    # Helper: try to copy a file if it exists
    function(copy_if_exists SRC_PATH DST_DIR)
        if(EXISTS "${SRC_PATH}")
            add_custom_command(TARGET NetWire POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${SRC_PATH}" "${DST_DIR}"
                VERBATIM)
        endif()
    endfunction()

    # Ensure deploy dir exists and copy our exe there
    add_custom_command(TARGET NetWire POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${DEPLOY_DIR}"
        COMMAND ${CMAKE_COMMAND} -E copy "$<TARGET_FILE:NetWire>" "${DEPLOY_DIR}"
        VERBATIM)

    # Try to locate Visual Studio installation to satisfy windeployqt --compiler-runtime
    set(VS_CANDIDATE_DIRS
        "C:/Program Files/Microsoft Visual Studio/2022/Community/VC"
        "C:/Program Files/Microsoft Visual Studio/2022/BuildTools/VC"
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/Community/VC"
        "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC"
    )
    foreach(_vsdir ${VS_CANDIDATE_DIRS})
        if(EXISTS "${_vsdir}")
            set(VCINSTALLDIR_HINT "${_vsdir}" CACHE PATH "VS VC install dir" FORCE)
            message(STATUS "Detected Visual Studio VC at: ${VCINSTALLDIR_HINT}")
            break()
        endif()
    endforeach()

    # Try to detect the latest MSVC redist directory under the detected VS VC root
    if(DEFINED VCINSTALLDIR_HINT)
        set(_REDIST_BASE "${VCINSTALLDIR_HINT}/Redist/MSVC")
        if(EXISTS "${_REDIST_BASE}")
            file(GLOB _MSVC_REDIST_DIRS "${_REDIST_BASE}/*")
            list(SORT _MSVC_REDIST_DIRS)
            list(REVERSE _MSVC_REDIST_DIRS)
            list(GET _MSVC_REDIST_DIRS 0 _MSVC_REDIST_DIR)
            if(EXISTS "${_MSVC_REDIST_DIR}")
                set(VCToolsRedistDir_HINT "${_MSVC_REDIST_DIR}" CACHE PATH "VS VC redist dir" FORCE)
                message(STATUS "Detected MSVC redist at: ${VCToolsRedistDir_HINT}")
            endif()
        endif()
    endif()

    # Use windeployqt to gather Qt DLLs and plugins
    set(WINDEPLOYQT_EXE "${QT_ROOT}/bin/windeployqt.exe")
    if(EXISTS "${WINDEPLOYQT_EXE}")
        if(DEFINED VCINSTALLDIR_HINT)
            add_custom_command(TARGET NetWire POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E env VCINSTALLDIR=${VCINSTALLDIR_HINT}
                                            $<$<BOOL:${VCToolsRedistDir_HINT}>:VCToolsRedistDir=${VCToolsRedistDir_HINT}>
                        "${WINDEPLOYQT_EXE}"
                        --verbose 1
                        --dir "${DEPLOY_DIR}"
                        --compiler-runtime
                        "$<TARGET_FILE:NetWire>"
                WORKING_DIRECTORY "${DEPLOY_DIR}"
                COMMENT "Running windeployqt with VCINSTALLDIR to collect Qt runtime dependencies"
                VERBATIM)
        else()
            add_custom_command(TARGET NetWire POST_BUILD
                COMMAND "${WINDEPLOYQT_EXE}"
                        --verbose 1
                        --dir "${DEPLOY_DIR}"
                        --compiler-runtime
                        "$<TARGET_FILE:NetWire>"
                WORKING_DIRECTORY "${DEPLOY_DIR}"
                COMMENT "Running windeployqt to collect Qt runtime dependencies"
                VERBATIM)
        endif()
    else()
        message(WARNING "windeployqt.exe not found at ${WINDEPLOYQT_EXE}. Qt DLLs will not be auto-copied.")
    endif()

    # Optionally copy Npcap/WinPcap runtime DLLs if available (for packet capture)
    # Common locations for 64-bit
    set(NPCAP_PATHS
        "C:/Program Files/Npcap"
        "C:/Windows/System32"
        "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/installed/x64-windows/bin"
    )
    foreach(P ${NPCAP_PATHS})
        copy_if_exists("${P}/wpcap.dll" "${DEPLOY_DIR}")
        copy_if_exists("${P}/Packet.dll" "${DEPLOY_DIR}")
        copy_if_exists("${P}/pcap.dll" "${DEPLOY_DIR}")
    endforeach()

    # Provide a convenience custom target to only (re)run deployment
    add_custom_target(deploy
        DEPENDS NetWire
        COMMAND ${CMAKE_COMMAND} -E echo "Deployed to: ${DEPLOY_DIR}"
    )
endif()
