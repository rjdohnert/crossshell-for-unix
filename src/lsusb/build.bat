@echo off
setlocal enabledelayedexpansion

echo [lsusb] Building lsusb...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=lsusb.cpp lsusb_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS=-lsetupapi"
set "MSVC_LIBS=setupapi.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsusb] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o lsusb.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lsusb] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsusb] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:lsusb.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [lsusb] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsusb] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o lsusb.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lsusb] Build failed with g++
    exit /b 1
)

echo [lsusb] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [lsusb] Build successful: lsusb.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y lsusb.exe "..\..\bin\lsusb.exe" >nul
    echo [lsusb] Installed to bin\lsusb.exe
)
:done
exit /b 0

:clean
echo [lsusb] Cleaning build artifacts...
del /q lsusb.exe *.obj *.o *.pdb *.ilk 2>nul
echo [lsusb] Clean complete.
exit /b 0
