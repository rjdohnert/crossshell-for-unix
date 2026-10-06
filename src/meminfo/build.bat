@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=meminfo.exe"
set "SOURCES=meminfo.cpp meminfo_app.cpp engine.cpp reporter.cpp options.cpp"

if "%1"=="clean" (
    echo [meminfo] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [meminfo] Clean complete.
    exit /b 0
)

echo [meminfo] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [meminfo] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -lpsapi -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [meminfo] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -lpsapi -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [meminfo] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% psapi.lib /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [meminfo] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [meminfo] Build failed!
    exit /b 1
)

echo [meminfo] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [meminfo] Installed to bin\%TARGET%
    )
)
