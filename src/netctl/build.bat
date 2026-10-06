@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=netctl.exe"
set "SOURCES=netctl.cpp netctl_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [netctl] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [netctl] Clean complete.
    exit /b 0
)

echo [netctl] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [netctl] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -lws2_32 -liphlpapi -ladvapi32 -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [netctl] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -lws2_32 -liphlpapi -ladvapi32 -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [netctl] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% ws2_32.lib iphlpapi.lib advapi32.lib /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [netctl] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [netctl] Build failed!
    exit /b 1
)

echo [netctl] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [netctl] Installed to bin\%TARGET%
    )
)
