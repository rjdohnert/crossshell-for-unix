@echo off
setlocal enabledelayedexpansion

set TARGET=setfacl.exe
set SOURCES=setfacl.cpp principal_resolver.cpp acl_engine.cpp setfacl_options.cpp
set BIN_DIR=..\..\bin

if "%1"=="clean" (
    echo [setfacl] Cleaning build artifacts...
    del /q /f *.obj *.o *.pdb %TARGET% 2>nul
    echo [setfacl] Clean complete.
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
    echo [setfacl] Error: No suitable C++ compiler found.
    exit /b 1
)

echo [setfacl] Building using %COMPILER%...

if "%COMPILER%"=="clang" (
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -ladvapi32 -o %TARGET%
) else if "%COMPILER%"=="gcc" (
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% -ladvapi32 -o %TARGET%
) else if "%COMPILER%"=="cl" (
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% advapi32.lib /Fe:%TARGET%
    del /q /f *.obj 2>nul
)

if not exist %TARGET% (
    echo [setfacl] Build failed.
    exit /b 1
)

echo [setfacl] Successfully built %TARGET%

if not "%1"=="--no-install" (
    if exist %BIN_DIR% (
        copy /y %TARGET% %BIN_DIR%\%TARGET% >nul
        echo [setfacl] Installed to %BIN_DIR%\%TARGET%
    )
)
