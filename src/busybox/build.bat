@echo off
setlocal

echo [busybox] Building busybox...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

if "%1"=="clean" goto :clean

where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [busybox] Compiling with clang++...
    clang++ -std=c++17 -O2 busybox.cpp -ladvapi32 -lcrypt32 -o busybox.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [busybox] Build failed with clang++.
    exit /b 1
)

where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [busybox] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 busybox.cpp /Fe:busybox.exe
    if %ERRORLEVEL% equ 0 (
        del /q *.obj 2>nul
        goto :success
    )
    echo [busybox] Build failed with cl.exe.
    exit /b 1
)

where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [busybox] Compiling with g++...
    g++ -std=c++17 -O2 busybox.cpp -ladvapi32 -lcrypt32 -o busybox.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [busybox] Build failed with g++.
    exit /b 1
)

echo [busybox] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [busybox] Build successful: busybox.exe
if exist "..\..\bin" (
    copy /y busybox.exe "..\..\bin\busybox.exe" >nul
    echo [busybox] Installed to bin\busybox.exe
)
exit /b 0

:clean
echo [busybox] Cleaning build artifacts...
del /q busybox.exe *.obj *.o *.pdb *.ilk 2>nul
echo [busybox] Clean complete.
exit /b 0