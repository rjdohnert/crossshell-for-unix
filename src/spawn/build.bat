@echo off
setlocal enabledelayedexpansion

set TARGET=spawn.exe
set SOURCES=spawn.cpp vms_status.cpp spawn_options.cpp process_controller.cpp spawn_engine.cpp
set BIN_DIR=..\..\bin

if "%1"=="clean" (
    echo [spawn] Cleaning build artifacts...
    del /q /f *.obj *.o *.pdb %TARGET% 2>nul
    echo [spawn] Clean complete.
    exit /b 0
)

set COMPILER=
where clang++ >nul 2>&1 && set COMPILER=clang
if not defined COMPILER (
    where g++ >nul 2>&1 && set COMPILER=gcc
)
if not defined COMPILER (
    where cl >nul 2>&1 && set COMPILER=cl
)
if not defined COMPILER (
    echo [spawn] Error: No suitable C++ compiler found.
    exit /b 1
)

echo [spawn] Building using %COMPILER%...

if "%COMPILER%"=="clang" (
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -o %TARGET%
) else if "%COMPILER%"=="gcc" (
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% -o %TARGET%
) else if "%COMPILER%"=="cl" (
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% /Fe:%TARGET%
    del /q /f *.obj 2>nul
)

if not exist %TARGET% (
    echo [spawn] Build failed.
    exit /b 1
)

echo [spawn] Successfully built %TARGET%

if not "%1"=="--no-install" (
    if exist %BIN_DIR% (
        copy /y %TARGET% %BIN_DIR%\%TARGET% >nul
        echo [spawn] Installed to %BIN_DIR%\%TARGET%
    )
)
