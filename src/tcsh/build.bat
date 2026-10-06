@echo off
setlocal enabledelayedexpansion

set TARGET=tcsh.exe
set SOURCES=tcsh.cpp tcsh_app.cpp options.cpp engine.cpp parser.cpp expansion.cpp builtins.cpp jobs.cpp terminal.cpp scripting.cpp selftest.cpp
set LIBS_MSVC=shell32.lib user32.lib advapi32.lib
set LIBS_GCC=-lshell32 -luser32 -ladvapi32
set BIN_DIR=..\..\bin

if "%1"=="clean" (
    echo [tcsh] Cleaning build artifacts...
    del /q /f *.obj *.o *.pdb %TARGET% 2>nul
    echo [tcsh] Clean complete.
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
    echo [tcsh] Error: No suitable C++ compiler found (clang++, g++, cl.exe).
    exit /b 1
)

echo [tcsh] Building using %COMPILER%...

if "%COMPILER%"=="clang" (
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% %LIBS_GCC% -o %TARGET%
) else if "%COMPILER%"=="gcc" (
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% %LIBS_GCC% -o %TARGET%
) else if "%COMPILER%"=="cl" (
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% %LIBS_MSVC% /Fe:%TARGET%
    del /q /f *.obj 2>nul
)

if not exist %TARGET% (
    echo [tcsh] Build failed.
    exit /b 1
)

echo [tcsh] Successfully built %TARGET%

if "%1"=="test" (
    echo [tcsh] Running internal self-tests...
    %TARGET% --self-test
)

if not "%1"=="--no-install" (
    if exist %BIN_DIR% (
        copy /y %TARGET% %BIN_DIR%\%TARGET% >nul
        echo [tcsh] Installed to %BIN_DIR%\%TARGET%
    )
)
