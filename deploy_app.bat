@echo off
echo NetWire Application Deployment Script
echo ===================================

REM Set Qt6 paths
set QT_BIN=C:\Qt\6.9.1\msvc2022_64\bin
set QT_PLUGINS=C:\Qt\6.9.1\msvc2022_64\plugins
set BUILD_DIR=build\Debug
set VCKG_BIN=vcpkg\installed\x64-windows\bin

echo Checking Qt6 installation...
if not exist "%QT_BIN%\Qt6Cored.dll" (
    echo ERROR: Qt6 installation not found at %QT_BIN%
    echo Please install Qt6.9.1 or update the path in this script
    pause
    exit /b 1
)

echo Creating deployment directory...
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"

echo Copying Qt6 core DLLs...
copy "%QT_BIN%\Qt6Cored.dll" "%BUILD_DIR%\"
copy "%QT_BIN%\Qt6Guid.dll" "%BUILD_DIR%\"
copy "%QT_BIN%\Qt6Widgetsd.dll" "%BUILD_DIR%\"
copy "%QT_BIN%\Qt6Networkd.dll" "%BUILD_DIR%\"
copy "%QT_BIN%\Qt6Chartsd.dll" "%BUILD_DIR%\"
copy "%QT_BIN%\Qt6Concurrentd.dll" "%BUILD_DIR%\"
copy "%QT_BIN%\Qt6OpenGLd.dll" "%BUILD_DIR%\"
copy "%QT_BIN%\Qt6OpenGLWidgetsd.dll" "%BUILD_DIR%\"

echo Creating plugin directories...
mkdir "%BUILD_DIR%\platforms" 2>nul
mkdir "%BUILD_DIR%\imageformats" 2>nul
mkdir "%BUILD_DIR%\iconengines" 2>nul
mkdir "%BUILD_DIR%\styles" 2>nul

echo Copying Qt6 plugins...
copy "%QT_PLUGINS%\platforms\*.dll" "%BUILD_DIR%\platforms\"
copy "%QT_PLUGINS%\imageformats\*.dll" "%BUILD_DIR%\imageformats\"
copy "%QT_PLUGINS%\iconengines\*.dll" "%BUILD_DIR%\iconengines\"
copy "%QT_PLUGINS%\styles\*.dll" "%BUILD_DIR%\styles\"

echo Copying pcap.dll...
if exist "%VCKG_BIN%\pcap.dll" (
    copy "%VCKG_BIN%\pcap.dll" "%BUILD_DIR%\"
    echo pcap.dll copied successfully
) else (
    echo WARNING: pcap.dll not found at %VCKG_BIN%
    echo Network monitoring features may not work
)

echo.
echo Deployment completed successfully!
echo.
echo Files deployed:
echo - Qt6 core DLLs: 8 files
echo - Qt6 plugins: platforms, imageformats, iconengines, styles
echo - pcap.dll: network monitoring support
echo.
echo You can now run the application using: run_app.bat
echo.
pause
