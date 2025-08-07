# Building NetWire Application

This document provides step-by-step instructions for building the NetWire network monitoring application on Windows.

## Prerequisites

1. **Visual Studio 2022** with C++ development tools
2. **Qt 6.9.1** (or compatible version) installed at `C:\Qt\6.9.1\msvc2022_64`
3. **CMake 3.20** or higher
4. **Npcap** or **libpcap** for packet capture functionality

## Build Instructions

### Method 1: Using Visual Studio Generator (Recommended)

1. Open a terminal (PowerShell or Command Prompt) and navigate to the project root:
   ```bash
   cd d:\project\netwire
   ```

2. Create a build directory (if not already present):
   ```bash
   mkdir build
   ```

3. Navigate to the build directory:
   ```bash
   cd build
   ```

4. Configure the project with CMake:
   ```bash
   cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="C:/Qt/6.9.1/msvc2022_64/lib/cmake/Qt6" ..
   ```

5. Build the project:
   ```bash
   cmake --build . --config Release
   ```
   
   Or for Debug build:
   ```bash
   cmake --build . --config Debug
   ```

### Method 2: Using vcpkg for Dependencies

If you have vcpkg installed and want to use it for managing dependencies:

1. Open a terminal and navigate to the project root:
   ```bash
   cd d:\project\netwire
   ```

2. Create a build directory:
   ```bash
   mkdir build
   cd build
   ```

3. Configure with vcpkg toolchain:
   ```bash
   cmake .. -DCMAKE_TOOLCHAIN_FILE=[path-to-vcpkg]/scripts/buildsystems/vcpkg.cmake -DCMAKE_PREFIX_PATH="C:/Qt/6.9.1/msvc2022_64/lib/cmake/Qt6" -DCMAKE_BUILD_TYPE=Release
   ```

4. Build the project:
   ```bash
   cmake --build . --config Release
   ```

## Common Issues and Solutions

### Qt6 Not Found
If CMake reports that Qt6 is not found, ensure:
- Qt is installed correctly
- The path in `-DCMAKE_PREFIX_PATH` is correct
- All required Qt6 components (Widgets, Network, Core, Gui, Charts, Concurrent) are installed

### Npcap/libpcap Issues
The application requires packet capture capabilities:
- Install Npcap on Windows for native support
- Alternatively, use libpcap through vcpkg:
  ```bash
  vcpkg install libpcap
  ```

## Output Location

After successful compilation, the executable will be located at:
- Release build: `build\Release\NetWire.exe`
- Debug build: `build\Debug\NetWire.exe`

## Running the Application

Navigate to the output directory and run:
```bash
cd Release
.\NetWire.exe
```

Or for Debug version:
```bash
cd Debug
.\NetWire.exe
```
