@echo off
setlocal enabledelayedexpansion

set TARGET=sed.exe
set SOURCES=sed.cpp sed_app.cpp engine.cpp parser.cpp options.cpp
set LIBS_MSVC=ole32.lib oleaut32.lib wbemuuid.lib advapi32.lib
set LIBS_GCC=-lole32 -loleaut32 -lwbemuuid -ladvapi32
set BIN_DIR=..\..\bin

if "%1"=="clean" (
    echo [sed] Cleaning build artifacts...
    del /q /f *.obj *.o *.pdb %TARGET% 2>nul
    echo [sed] Clean complete.
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
    echo [sed] Error: No suitable C++ compiler found (clang++, g++, cl.exe).
    exit /b 1
)

echo [sed] Building using %COMPILER%...

if "%COMPILER%"=="clang" (
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% %LIBS_GCC% -o %TARGET%
) else if "%COMPILER%"=="gcc" (
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% %LIBS_GCC% -o %TARGET%
) else if "%COMPILER%"=="cl" (
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% %LIBS_MSVC% /Fe:%TARGET%
    del /q /f *.obj 2>nul
)

if not exist %TARGET% (
    echo [sed] Build failed.
    exit /b 1
)

echo [sed] Successfully built %TARGET%

if not "%1"=="--no-install" (
    if exist %BIN_DIR% (
        copy /y %TARGET% %BIN_DIR%\%TARGET% >nul
        echo [sed] Installed to %BIN_DIR%\%TARGET%
    )
)
