@echo off
setlocal enabledelayedexpansion

set "TARGET=dirtree"
set "BIN_DIR=..\..\bin"
set "SRC=dirtree.cpp dirtree_app.cpp traversal_engine.cpp path_helper.cpp options.cpp"

if not exist "!BIN_DIR!" mkdir "!BIN_DIR!"

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo Building !TARGET! with clang++...
    clang++ -std=c++17 -O2 !SRC! -o "!BIN_DIR!\!TARGET!.exe"
    if %errorlevel% equ 0 (
        echo Build successful: !BIN_DIR!\!TARGET!.exe
        exit /b 0
    )
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo Building !TARGET! with MSVC cl...
    cl /std:c++17 /O2 /EHsc /Fe:"!BIN_DIR!\!TARGET!.exe" !SRC!
    if %errorlevel% equ 0 (
        echo Build successful: !BIN_DIR!\!TARGET!.exe
        if exist "*.obj" del *.obj
        exit /b 0
    )
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo Building !TARGET! with g++...
    g++ -std=c++17 -O2 !SRC! -o "!BIN_DIR!\!TARGET!.exe"
    if %errorlevel% equ 0 (
        echo Build successful: !BIN_DIR!\!TARGET!.exe
        exit /b 0
    )
)

echo Error: No suitable C++ compiler found (clang++, cl, or g++).
exit /b 1
