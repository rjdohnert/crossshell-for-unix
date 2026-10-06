@echo off
setlocal enabledelayedexpansion

set TARGET=sequence.exe
set SOURCES=sequence.cpp sequence_options.cpp field_extractor.cpp key_comparator.cpp sort_pipeline.cpp
set BIN_DIR=..\..\bin

if "%1"=="clean" (
    echo [sequence] Cleaning build artifacts...
    del /q /f *.obj *.o *.pdb %TARGET% 2>nul
    echo [sequence] Clean complete.
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
    echo [sequence] Error: No suitable C++ compiler found.
    exit /b 1
)

echo [sequence] Building using %COMPILER%...

if "%COMPILER%"=="clang" (
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -o %TARGET%
) else if "%COMPILER%"=="gcc" (
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% -o %TARGET%
) else if "%COMPILER%"=="cl" (
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% /Fe:%TARGET%
    del /q /f *.obj 2>nul
)

if not exist %TARGET% (
    echo [sequence] Build failed.
    exit /b 1
)

echo [sequence] Successfully built %TARGET%

if not "%1"=="--no-install" (
    if exist %BIN_DIR% (
        copy /y %TARGET% %BIN_DIR%\%TARGET% >nul
        echo [sequence] Installed to %BIN_DIR%\%TARGET%
    )
)
