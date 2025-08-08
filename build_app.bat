@echo off
echo Building NetWire...
cd /d "%~dp0"

echo Configuring project...
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

if %ERRORLEVEL% NEQ 0 (
    echo Configuration failed!
    pause
    exit /b 1
)

echo Building project...
cmake --build build --config Debug

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo Build completed successfully!
echo You can now run the application with: run_app.bat
pause
