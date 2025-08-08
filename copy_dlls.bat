@echo off
echo Copying required DLLs to build directory...

set QT_ROOT=C:\Qt\6.9.1\msvc2022_64
set BUILD_DIR=build\Debug

echo Copying Qt6 DLLs to %BUILD_DIR%...
copy "%QT_ROOT%\bin\Qt6Cored.dll" "%BUILD_DIR%\" 2>nul
copy "%QT_ROOT%\bin\Qt6Guid.dll" "%BUILD_DIR%\" 2>nul
copy "%QT_ROOT%\bin\Qt6Widgetsd.dll" "%BUILD_DIR%\" 2>nul
copy "%QT_ROOT%\bin\Qt6Networkd.dll" "%BUILD_DIR%\" 2>nul
copy "%QT_ROOT%\bin\Qt6Chartsd.dll" "%BUILD_DIR%\" 2>nul
copy "%QT_ROOT%\bin\Qt6Concurrentd.dll" "%BUILD_DIR%\" 2>nul

echo Copying Qt6 platform plugins...
if not exist "%BUILD_DIR%\platforms" mkdir "%BUILD_DIR%\platforms"
copy "%QT_ROOT%\plugins\platforms\qwindowsd.dll" "%BUILD_DIR%\platforms\" 2>nul

echo Copying Qt6 image format plugins...
if not exist "%BUILD_DIR%\imageformats" mkdir "%BUILD_DIR%\imageformats"
copy "%QT_ROOT%\plugins\imageformats\qicod.dll" "%BUILD_DIR%\imageformats\" 2>nul
copy "%QT_ROOT%\plugins\imageformats\qjpegd.dll" "%BUILD_DIR%\imageformats\" 2>nul
copy "%QT_ROOT%\plugins\imageformats\qpngd.dll" "%BUILD_DIR%\imageformats\" 2>nul
copy "%QT_ROOT%\plugins\imageformats\qsvgd.dll" "%BUILD_DIR%\imageformats\" 2>nul

echo Copying Qt6 icon engine plugins...
if not exist "%BUILD_DIR%\iconengines" mkdir "%BUILD_DIR%\iconengines"
copy "%QT_ROOT%\plugins\iconengines\qsvgicond.dll" "%BUILD_DIR%\iconengines\" 2>nul

echo Copying Qt6 style plugins...
if not exist "%BUILD_DIR%\styles" mkdir "%BUILD_DIR%\styles"
copy "%QT_ROOT%\plugins\styles\qwindowsvistastyled.dll" "%BUILD_DIR%\styles\" 2>nul

echo Copying Qt6OpenGL DLLs...
copy "%QT_ROOT%\bin\Qt6OpenGLd.dll" "%BUILD_DIR%\" 2>nul
copy "%QT_ROOT%\bin\Qt6OpenGLWidgetsd.dll" "%BUILD_DIR%\" 2>nul

echo Copying libpcap DLL...
if exist "vcpkg\installed\x64-windows\bin\pcap.dll" (
    copy "vcpkg\installed\x64-windows\bin\pcap.dll" "%BUILD_DIR%\" 2>nul
    echo pcap.dll copied successfully
) else (
    echo Warning: pcap.dll not found in vcpkg directory
    echo Please install Npcap or ensure libpcap is properly installed
)

echo DLL copying complete!
echo You can now run: .\build\Debug\NetWire.exe
pause
