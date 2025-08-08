@echo off
echo Running NetWire as Administrator...

:: Check if running as administrator
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Already running as administrator.
    goto :run_app
) else (
    echo Requesting administrator privileges...
    powershell -Command "Start-Process '%~dp0build\Debug\NetWire.exe' -Verb RunAs"
    goto :end
)

:run_app
echo Starting NetWire...
"%~dp0build\Debug\NetWire.exe"

:end
pause
