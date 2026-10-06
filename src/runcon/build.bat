@echo off
setlocal enabledelayedexpansion

set TARGET=runcon.exe
set SOURCES=runcon.cpp runcon_options.cpp token_privilege.cpp process_launcher.cpp
set LIBS_MSVC=advapi32.lib
set LIBS_GCC=-ladvapi32
set BIN_DIR=..\..\bin

if "%1"=="clean" (
    echo [runcon] Cleaning build artifacts...
    del /q /f *.obj *.o *.pdb %TARGET% 2>nul
    echo [runcon] Clean complete.
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
    echo [runcon] Error: No suitable C++ compiler found.
    exit /b 1
)

echo [runcon] Building using %COMPILER%...

if "%COMPILER%"=="clang" (
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% %LIBS_GCC% -o %TARGET%
) else if "%COMPILER%"=="gcc" (
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% %LIBS_GCC% -o %TARGET%
) else if "%COMPILER%"=="cl" (
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% %LIBS_MSVC% /Fe:%TARGET%
    del /q /f *.obj 2>nul
)

if not exist %TARGET% (
    echo [runcon] Build failed.
    exit /b 1
)

echo [runcon] Successfully built %TARGET%

if not "%1"=="--no-install" (
    if exist %BIN_DIR% (
        copy /y %TARGET% %BIN_DIR%\%TARGET% >nul
        echo [runcon] Installed to %BIN_DIR%\%TARGET%
    )
)
