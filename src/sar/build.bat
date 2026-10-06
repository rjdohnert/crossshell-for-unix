@echo off
setlocal enabledelayedexpansion

set TARGET=sar.exe
set SOURCES=sar.cpp sar_options.cpp system_sampler.cpp report_formatter.cpp
set LIBS_MSVC=pdh.lib iphlpapi.lib psapi.lib
set LIBS_GCC=-lpdh -liphlpapi -lpsapi
set BIN_DIR=..\..\bin

if "%1"=="clean" (
    echo [sar] Cleaning build artifacts...
    del /q /f *.obj *.o *.pdb %TARGET% 2>nul
    echo [sar] Clean complete.
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
    echo [sar] Error: No suitable C++ compiler found.
    exit /b 1
)

echo [sar] Building using %COMPILER%...

if "%COMPILER%"=="clang" (
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% %LIBS_GCC% -o %TARGET%
) else if "%COMPILER%"=="gcc" (
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% %LIBS_GCC% -o %TARGET%
) else if "%COMPILER%"=="cl" (
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% %LIBS_MSVC% /Fe:%TARGET%
    del /q /f *.obj 2>nul
)

if not exist %TARGET% (
    echo [sar] Build failed.
    exit /b 1
)

echo [sar] Successfully built %TARGET%

if not "%1"=="--no-install" (
    if exist %BIN_DIR% (
        copy /y %TARGET% %BIN_DIR%\%TARGET% >nul
        echo [sar] Installed to %BIN_DIR%\%TARGET%
    )
)
