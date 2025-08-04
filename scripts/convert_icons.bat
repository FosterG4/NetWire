@echo off
setlocal enabledelayedexpansion

REM Create output directory if it doesn't exist
if not exist "resources\icons\png" mkdir "resources\icons\png"

REM Check if Python is available
where python >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Python is not in your PATH. Please install Python and add it to your PATH.
    exit /b 1
)

REM Install required packages
echo Installing required Python packages...
pip install PyQt6

REM Run the conversion script
echo Converting SVG icons to PNG...
python -c "from PyQt6.QtWidgets import QApplication; app = QApplication([]); print('Qt application initialized')"
if %ERRORLEVEL% NEQ 0 (
    echo Failed to initialize Qt application. Make sure you have all required dependencies installed.
    exit /b 1
)

python scripts/convert_icons.py

if %ERRORLEVEL% EQU 0 (
    echo Icons have been successfully converted to PNG format.
) else (
    echo Failed to convert icons. Please check the error messages above.
    exit /b 1
)

endlocal
