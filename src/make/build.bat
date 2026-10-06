@echo off
setlocal enabledelayedexpansion

echo [make] Building make...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=make.cpp make_app.cpp engine.cpp options.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [make] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -Wl,/subsystem:console -o make.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [make] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [make] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:make.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [make] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [make] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% -o make.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [make] Build failed with g++
    exit /b 1
)

echo [make] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [make] Build successful: make.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y make.exe "..\..\bin\make.exe" >nul
    echo [make] Installed to bin\make.exe
)
:done
exit /b 0

:clean
echo [make] Cleaning build artifacts...
del /q make.exe *.obj *.o *.pdb *.ilk 2>nul
echo [make] Clean complete.
exit /b 0
