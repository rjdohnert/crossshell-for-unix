@echo off
setlocal enabledelayedexpansion

echo [basename] Building basename...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=basename.cpp basename_app.cpp engine.cpp options.cpp reporter.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [basename] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -o basename.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [basename] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [basename] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:basename.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [basename] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [basename] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% -o basename.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [basename] Build failed with g++
    exit /b 1
)

echo [basename] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [basename] Build successful: basename.exe
if exist "..\..\bin" (
    copy /y basename.exe "..\..\bin\basename.exe" >nul
    echo [basename] Installed to bin\basename.exe
)
exit /b 0

:clean
echo [basename] Cleaning build artifacts...
del /q basename.exe *.obj *.o *.pdb *.ilk 2>nul
echo [basename] Clean complete.
exit /b 0
