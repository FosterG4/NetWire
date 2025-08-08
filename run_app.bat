@echo off
echo Starting NetWire...
cd /d "%~dp0"

REM Copy required Qt6 DLLs if they don't exist
if not exist "build\Debug\Qt6Cored.dll" (
    echo Copying Qt6 DLLs...
    copy "C:\Qt\6.9.1\msvc2022_64\bin\Qt6Cored.dll" "build\Debug\"
    copy "C:\Qt\6.9.1\msvc2022_64\bin\Qt6Guid.dll" "build\Debug\"
    copy "C:\Qt\6.9.1\msvc2022_64\bin\Qt6Widgetsd.dll" "build\Debug\"
    copy "C:\Qt\6.9.1\msvc2022_64\bin\Qt6Networkd.dll" "build\Debug\"
    copy "C:\Qt\6.9.1\msvc2022_64\bin\Qt6Chartsd.dll" "build\Debug\"
    copy "C:\Qt\6.9.1\msvc2022_64\bin\Qt6Concurrentd.dll" "build\Debug\"
    copy "C:\Qt\6.9.1\msvc2022_64\bin\Qt6OpenGLd.dll" "build\Debug\"
    copy "C:\Qt\6.9.1\msvc2022_64\bin\Qt6OpenGLWidgetsd.dll" "build\Debug\"
)

REM Copy Qt6 plugins if they don't exist
if not exist "build\Debug\platforms\qwindowsd.dll" (
    echo Copying Qt6 plugins...
    mkdir "build\Debug\platforms" 2>nul
    copy "C:\Qt\6.9.1\msvc2022_64\plugins\platforms\*.dll" "build\Debug\platforms\"
    
    mkdir "build\Debug\imageformats" 2>nul
    copy "C:\Qt\6.9.1\msvc2022_64\plugins\imageformats\*.dll" "build\Debug\imageformats\"
    
    mkdir "build\Debug\iconengines" 2>nul
    copy "C:\Qt\6.9.1\msvc2022_64\plugins\iconengines\*.dll" "build\Debug\iconengines\"
    
    mkdir "build\Debug\styles" 2>nul
    copy "C:\Qt\6.9.1\msvc2022_64\plugins\styles\*.dll" "build\Debug\styles\"
)

REM Copy pcap.dll if it doesn't exist
if not exist "build\Debug\pcap.dll" (
    echo Copying pcap.dll...
    copy "vcpkg\installed\x64-windows\bin\pcap.dll" "build\Debug\"
)

REM Copy MSVC runtime DLLs if needed
if not exist "build\Debug\msvcp140d.dll" (
    echo Copying MSVC runtime DLLs...
    copy "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Redist\MSVC\14.29.30133\x64\Microsoft.VC142.CRT\msvcp140d.dll" "build\Debug\" 2>nul
    copy "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Redist\MSVC\14.29.30133\x64\Microsoft.VC142.CRT\vcruntime140d.dll" "build\Debug\" 2>nul
    copy "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Redist\MSVC\14.29.30133\x64\Microsoft.VC142.CRT\vcruntime140_1d.dll" "build\Debug\" 2>nul
)

if exist "build\Debug\NetWire.exe" (
    echo Found NetWire executable
    echo Starting NetWire application...
    start "" "build\Debug\NetWire.exe"
) else (
    echo NetWire executable not found. Please build the project first.
    echo Run: build_app.bat
    pause
) 