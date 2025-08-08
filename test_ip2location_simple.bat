@echo off
echo Building simple IP2Location test...

REM Create build directory
if not exist "test_ip2location_simple_build" mkdir test_ip2location_simple_build
cd test_ip2location_simple_build

REM Copy CMakeLists.txt to build directory
copy ..\test_ip2location_simple_CMakeLists.txt CMakeLists.txt

REM Configure with CMake
cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Debug .

REM Build the test
cmake --build . --config Debug

REM Run the test
echo.
echo Running simple IP2Location test...
echo.
tests\Debug\TestIP2LocationSimple.exe

echo.
echo Test completed.
pause
