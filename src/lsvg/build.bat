@echo off
setlocal enabledelayedexpansion

echo [lsvg] Building lsvg...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=lsvg.cpp lsvg_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS="
set "MSVC_LIBS="

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsvg] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -Wl,/subsystem:console -o lsvg.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lsvg] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsvg] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:lsvg.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [lsvg] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsvg] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% -o lsvg.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lsvg] Build failed with g++
    exit /b 1
)

echo [lsvg] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [lsvg] Build successful: lsvg.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y lsvg.exe "..\..\bin\lsvg.exe" >nul
    echo [lsvg] Installed to bin\lsvg.exe
)
:done
exit /b 0

:clean
echo [lsvg] Cleaning build artifacts...
del /q lsvg.exe *.obj *.o *.pdb *.ilk 2>nul
echo [lsvg] Clean complete.
exit /b 0
