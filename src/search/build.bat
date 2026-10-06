@echo off
setlocal enabledelayedexpansion

set TARGET=search.exe
set SOURCES=search.cpp terminal_format.cpp known_folders.cpp pattern_matcher.cpp search_options.cpp search_reporter.cpp search_engine.cpp
set LIBS_MSVC=Shell32.lib Ole32.lib
set LIBS_GCC=-lshell32 -lole32
set BIN_DIR=..\..\bin

if "%1"=="clean" (
    echo [search] Cleaning build artifacts...
    del /q /f *.obj *.o *.pdb %TARGET% 2>nul
    echo [search] Clean complete.
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
    echo [search] Error: No suitable C++ compiler found.
    exit /b 1
)

echo [search] Building using %COMPILER%...

if "%COMPILER%"=="clang" (
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% %LIBS_GCC% -o %TARGET%
) else if "%COMPILER%"=="gcc" (
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% %LIBS_GCC% -o %TARGET%
) else if "%COMPILER%"=="cl" (
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% %LIBS_MSVC% /Fe:%TARGET%
    del /q /f *.obj 2>nul
)

if not exist %TARGET% (
    echo [search] Build failed.
    exit /b 1
)

echo [search] Successfully built %TARGET%

if not "%1"=="--no-install" (
    if exist %BIN_DIR% (
        copy /y %TARGET% %BIN_DIR%\%TARGET% >nul
        echo [search] Installed to %BIN_DIR%\%TARGET%
    )
)
