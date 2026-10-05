@echo off
setlocal enabledelayedexpansion

echo [clock] Building clock...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=clock.cpp clock_app.cpp engine.cpp options.cpp reporter.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [clock] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -o clock.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [clock] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [clock] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:clock.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [clock] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [clock] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% -o clock.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [clock] Build failed with g++
    exit /b 1
)

echo [clock] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [clock] Build successful: clock.exe
if exist "..\..\bin" (
    copy /y clock.exe "..\..\bin\clock.exe" >nul
    echo [clock] Installed to bin\clock.exe
)
exit /b 0

:clean
echo [clock] Cleaning build artifacts...
del /q clock.exe *.obj *.o *.pdb *.ilk 2>nul
echo [clock] Clean complete.
exit /b 0
