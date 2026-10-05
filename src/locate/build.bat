@echo off
setlocal enabledelayedexpansion

echo [locate] Building locate...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=locate.cpp locate_app.cpp engine.cpp options.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [locate] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -Wl,/subsystem:console -o locate.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [locate] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [locate] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:locate.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [locate] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [locate] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% -o locate.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [locate] Build failed with g++
    exit /b 1
)

echo [locate] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [locate] Build successful: locate.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y locate.exe "..\..\bin\locate.exe" >nul
    echo [locate] Installed to bin\locate.exe
)
:done
exit /b 0

:clean
echo [locate] Cleaning build artifacts...
del /q locate.exe *.obj *.o *.pdb *.ilk 2>nul
echo [locate] Clean complete.
exit /b 0
