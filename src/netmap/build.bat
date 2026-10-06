@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=netmap.exe"
set "SOURCES=netmap.cpp netmap_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [netmap] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [netmap] Clean complete.
    exit /b 0
)

echo [netmap] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [netmap] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -liphlpapi -lws2_32 -ladvapi32 -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [netmap] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -liphlpapi -lws2_32 -ladvapi32 -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [netmap] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% iphlpapi.lib ws2_32.lib advapi32.lib /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [netmap] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [netmap] Build failed!
    exit /b 1
)

echo [netmap] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [netmap] Installed to bin\%TARGET%
    )
)
