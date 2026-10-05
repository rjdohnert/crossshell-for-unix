@echo off
setlocal enabledelayedexpansion

echo [chroot] Building chroot...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=chroot.cpp chroot_app.cpp path_helper.cpp options.cpp executor.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chroot] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -o chroot.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [chroot] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chroot] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:chroot.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [chroot] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chroot] Compiling with g++...
    g++ -std=c++17 -O2 -municode %SOURCES% -o chroot.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [chroot] Build failed with g++
    exit /b 1
)

echo [chroot] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [chroot] Build successful: chroot.exe
if exist "..\..\bin" (
    copy /y chroot.exe "..\..\bin\chroot.exe" >nul
    echo [chroot] Installed to bin\chroot.exe
)
exit /b 0

:clean
echo [chroot] Cleaning build artifacts...
del /q chroot.exe *.obj *.o *.pdb *.ilk 2>nul
echo [chroot] Clean complete.
exit /b 0
