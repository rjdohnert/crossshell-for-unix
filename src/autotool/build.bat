@echo off
setlocal enabledelayedexpansion

echo [autotool] Building autotool...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=autoconf.cpp automake.cpp autotool.cpp autotool_app.cpp build_engine.cpp help.cpp libtool.cpp port_engine.cpp utils.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [autotool] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -o autotool.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [autotool] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [autotool] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:autotool.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [autotool] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [autotool] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% -o autotool.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [autotool] Build failed with g++
    exit /b 1
)

echo [autotool] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [autotool] Build successful: autotool.exe
if exist "..\..\bin" (
    copy /y autotool.exe "..\..\bin\autotool.exe" >nul
    echo [autotool] Installed to bin\autotool.exe
)
exit /b 0

:clean
echo [autotool] Cleaning build artifacts...
del /q autotool.exe *.obj *.o *.pdb *.ilk 2>nul
echo [autotool] Clean complete.
exit /b 0
