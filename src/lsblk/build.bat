@echo off
setlocal enabledelayedexpansion

echo [lsblk] Building lsblk...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=lsblk.cpp lsblk_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS=-lmpr"
set "MSVC_LIBS=mpr.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsblk] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o lsblk.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lsblk] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsblk] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:lsblk.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [lsblk] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsblk] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o lsblk.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lsblk] Build failed with g++
    exit /b 1
)

echo [lsblk] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [lsblk] Build successful: lsblk.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y lsblk.exe "..\..\bin\lsblk.exe" >nul
    echo [lsblk] Installed to bin\lsblk.exe
)
:done
exit /b 0

:clean
echo [lsblk] Cleaning build artifacts...
del /q lsblk.exe *.obj *.o *.pdb *.ilk 2>nul
echo [lsblk] Clean complete.
exit /b 0
