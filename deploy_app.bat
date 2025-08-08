@echo off
echo Deploying NetWire application...

set QT_ROOT=C:\Qt\6.9.1\msvc2022_64
set BUILD_DIR=build\Debug
set DEPLOY_DIR=deploy\NetWire

echo Creating deployment directory...
if not exist "%DEPLOY_DIR%" mkdir "%DEPLOY_DIR%"

echo Copying executable...
copy "%BUILD_DIR%\NetWire.exe" "%DEPLOY_DIR%\"

echo Copying Qt6 DLLs...
copy "%QT_ROOT%\bin\Qt6Cored.dll" "%DEPLOY_DIR%\"
copy "%QT_ROOT%\bin\Qt6Guid.dll" "%DEPLOY_DIR%\"
copy "%QT_ROOT%\bin\Qt6Widgetsd.dll" "%DEPLOY_DIR%\"
copy "%QT_ROOT%\bin\Qt6Networkd.dll" "%DEPLOY_DIR%\"
copy "%QT_ROOT%\bin\Qt6Chartsd.dll" "%DEPLOY_DIR%\"
copy "%QT_ROOT%\bin\Qt6Concurrentd.dll" "%DEPLOY_DIR%\"

echo Copying Qt6 platform plugins...
if not exist "%DEPLOY_DIR%\platforms" mkdir "%DEPLOY_DIR%\platforms"
copy "%QT_ROOT%\plugins\platforms\qwindowsd.dll" "%DEPLOY_DIR%\platforms\"

echo Copying Qt6 image format plugins...
if not exist "%DEPLOY_DIR%\imageformats" mkdir "%DEPLOY_DIR%\imageformats"
copy "%QT_ROOT%\plugins\imageformats\qicod.dll" "%DEPLOY_DIR%\imageformats\"
copy "%QT_ROOT%\plugins\imageformats\qjpegd.dll" "%DEPLOY_DIR%\imageformats\"
copy "%QT_ROOT%\plugins\imageformats\qpngd.dll" "%DEPLOY_DIR%\imageformats\"
copy "%QT_ROOT%\plugins\imageformats\qsvgd.dll" "%DEPLOY_DIR%\imageformats\"

echo Copying Qt6 icon engine plugins...
if not exist "%DEPLOY_DIR%\iconengines" mkdir "%DEPLOY_DIR%\iconengines"
copy "%QT_ROOT%\plugins\iconengines\qsvgicond.dll" "%DEPLOY_DIR%\iconengines\"

echo Copying Qt6 style plugins...
if not exist "%DEPLOY_DIR%\styles" mkdir "%DEPLOY_DIR%\styles"
copy "%QT_ROOT%\plugins\styles\qwindowsvistastyled.dll" "%DEPLOY_DIR%\styles\"

echo Copying libpcap DLL...
if exist "vcpkg\installed\x64-windows\bin\pcap.dll" (
    copy "vcpkg\installed\x64-windows\bin\pcap.dll" "%DEPLOY_DIR%\"
) else (
    echo Warning: pcap.dll not found in vcpkg directory
    echo Please install Npcap or ensure libpcap is properly installed
)

echo Copying resources...
if exist "resources" (
    xcopy "resources" "%DEPLOY_DIR%\resources\" /E /I /Y
)

echo Creating README...
echo NetWire - Network Monitor > "%DEPLOY_DIR%\README.txt"
echo. >> "%DEPLOY_DIR%\README.txt"
echo This application requires the following dependencies: >> "%DEPLOY_DIR%\README.txt"
echo - Qt6 Runtime Libraries >> "%DEPLOY_DIR%\README.txt"
echo - Npcap or WinPcap for network monitoring >> "%DEPLOY_DIR%\README.txt"
echo. >> "%DEPLOY_DIR%\README.txt"
echo If you encounter missing DLL errors: >> "%DEPLOY_DIR%\README.txt"
echo 1. Install Qt6 Runtime from https://www.qt.io/download >> "%DEPLOY_DIR%\README.txt"
echo 2. Install Npcap from https://npcap.com/ >> "%DEPLOY_DIR%\README.txt"
echo. >> "%DEPLOY_DIR%\README.txt"
echo Run NetWire.exe to start the application. >> "%DEPLOY_DIR%\README.txt"

echo Deployment complete!
echo Application is available in: %DEPLOY_DIR%
pause
