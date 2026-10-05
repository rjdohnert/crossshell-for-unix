@echo off
setlocal enabledelayedexpansion

echo [lsdev] Building lsdev...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=lsdev.cpp lsdev_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS=-lsetupapi -lcfgmgr32"
set "MSVC_LIBS=setupapi.lib cfgmgr32.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsdev] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o lsdev.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lsdev] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsdev] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:lsdev.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [lsdev] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsdev] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o lsdev.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lsdev] Build failed with g++
    exit /b 1
)

echo [lsdev] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [lsdev] Build successful: lsdev.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y lsdev.exe "..\..\bin\lsdev.exe" >nul
    echo [lsdev] Installed to bin\lsdev.exe
)
:done
exit /b 0

:clean
echo [lsdev] Cleaning build artifacts...
del /q lsdev.exe *.obj *.o *.pdb *.ilk 2>nul
echo [lsdev] Clean complete.
exit /b 0
