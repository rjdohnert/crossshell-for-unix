@echo off
setlocal enabledelayedexpansion

set TARGET=strace.exe
set SOURCES=strace.cpp strace_app.cpp engine.cpp reporter.cpp options.cpp
set LIBS_MSVC=dbghelp.lib
set LIBS_GCC=-ldbghelp -municode
set LIBS_CLANG=-ldbghelp -Wl,/subsystem:console
set BIN_DIR=..\..\bin

if "%1"=="clean" (
    echo [strace] Cleaning build artifacts...
    del /q /f *.obj *.o *.pdb %TARGET% 2>nul
    echo [strace] Clean complete.
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
    echo [strace] Error: No suitable C++ compiler found (clang++, g++, cl.exe).
    exit /b 1
)

echo [strace] Building using %COMPILER%...

if "%COMPILER%"=="clang" (
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% %LIBS_CLANG% -o %TARGET%
) else if "%COMPILER%"=="gcc" (
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% %LIBS_GCC% -o %TARGET%
) else if "%COMPILER%"=="cl" (
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% %LIBS_MSVC% /Fe:%TARGET%
    del /q /f *.obj 2>nul
)

if not exist %TARGET% (
    echo [strace] Build failed.
    exit /b 1
)

echo [strace] Successfully built %TARGET%

if not "%1"=="--no-install" (
    if exist %BIN_DIR% (
        copy /y %TARGET% %BIN_DIR%\%TARGET% >nul
        echo [strace] Installed to %BIN_DIR%\%TARGET%
    )
)
