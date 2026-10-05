@echo off
setlocal enabledelayedexpansion

set "TARGET=dig"
set "BIN_DIR=..\..\bin"
set "SRC=dig.cpp dig_app.cpp options.cpp query_engine.cpp reporter.cpp"
set "LIBS=-ldnsapi -lws2_32"
set "CL_LIBS=dnsapi.lib ws2_32.lib"

if not exist "!BIN_DIR!" mkdir "!BIN_DIR!"

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo Building !TARGET! with clang++...
    clang++ -std=c++17 -O2 !SRC! !LIBS! -o "!BIN_DIR!\!TARGET!.exe"
    if %errorlevel% equ 0 (
        echo Build successful: !BIN_DIR!\!TARGET!.exe
        exit /b 0
    )
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo Building !TARGET! with MSVC cl...
    cl /std:c++17 /O2 /EHsc /Fe:"!BIN_DIR!\!TARGET!.exe" !SRC! !CL_LIBS!
    if %errorlevel% equ 0 (
        echo Build successful: !BIN_DIR!\!TARGET!.exe
        if exist "*.obj" del *.obj
        exit /b 0
    )
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo Building !TARGET! with g++...
    g++ -std=c++17 -O2 !SRC! !LIBS! -o "!BIN_DIR!\!TARGET!.exe"
    if %errorlevel% equ 0 (
        echo Build successful: !BIN_DIR!\!TARGET!.exe
        exit /b 0
    )
)

echo Error: No suitable C++ compiler found (clang++, cl, or g++).
exit /b 1
