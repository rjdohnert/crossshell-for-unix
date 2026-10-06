@echo off
setlocal enabledelayedexpansion

set TARGET=zsh.exe
set SOURCES=engine.cpp parser.cpp expansion.cpp jobs.cpp terminal.cpp builtins.cpp scripting.cpp selftest.cpp zsh.cpp
set LIBS_MSVC=shell32.lib user32.lib advapi32.lib
set LIBS_GCC=-lshell32 -luser32 -ladvapi32 -Wl,/STACK:8388608
set BIN_DIR=..\..\bin

if "%1"=="clean" (
    echo [zsh] Cleaning build artifacts...
    del /q /f *.obj *.o *.pdb %TARGET% 2>nul
    echo [zsh] Clean complete.
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
    echo [zsh] Error: No suitable C++ compiler found.
    exit /b 1
)

echo [zsh] Building using %COMPILER%...

if "%COMPILER%"=="clang" (
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% %LIBS_GCC% -o %TARGET%
) else if "%COMPILER%"=="gcc" (
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% %LIBS_GCC% -o %TARGET%
) else if "%COMPILER%"=="cl" (
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% %LIBS_MSVC% /Fe:%TARGET% /link /STACK:8388608
    del /q /f *.obj 2>nul
)

if not exist %TARGET% (
    echo [zsh] Build failed.
    exit /b 1
)

echo [zsh] Successfully built %TARGET%

if "%1"=="test" (
    echo [zsh] Running internal self-tests...
    %TARGET% --self-test
)

if not "%1"=="--no-install" (
    if exist %BIN_DIR% (
        copy /y %TARGET% %BIN_DIR%\%TARGET% >nul
        echo [zsh] Installed to %BIN_DIR%\%TARGET%
    )
)
