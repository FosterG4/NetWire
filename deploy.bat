@echo off
echo Deploying NetWire Application Dependencies...

REM Check if build directory exists
if not exist "build\NetWire.exe" (
    echo Error: NetWire.exe not found in build directory
    echo Please build the project first: cmake --build build --config Debug
    pause
    exit /b 1
)

echo Step 1: Deploying Qt dependencies...
"C:\Qt\6.9.1\msvc2022_64\bin\windeployqt.exe" build\NetWire.exe

echo Step 2: Copying pcap.dll...
if exist "vcpkg\installed\x64-windows\bin\pcap.dll" (
    copy "vcpkg\installed\x64-windows\bin\pcap.dll" "build\"
    echo pcap.dll copied successfully
) else (
    echo Warning: pcap.dll not found in vcpkg directory
)

echo Step 3: Verifying deployment...
echo Checking required DLLs:
if exist "build\Qt6Cored.dll" (echo ✓ Qt6Cored.dll) else (echo ✗ Qt6Cored.dll missing)
if exist "build\Qt6Chartsd.dll" (echo ✓ Qt6Chartsd.dll) else (echo ✗ Qt6Chartsd.dll missing)
if exist "build\pcap.dll" (echo ✓ pcap.dll) else (echo ✗ pcap.dll missing)

echo.
echo Deployment completed! You can now run the application with:
echo   cd build && ./NetWire.exe
echo   or use: run_app.bat
pause 