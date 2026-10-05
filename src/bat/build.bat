@echo off
setlocal enabledelayedexpansion

echo [bat] Building bat...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=bat.cpp bat_app.cpp engine.cpp highlighter.cpp options.cpp reporter.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [bat] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -o bat.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [bat] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [bat] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:bat.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [bat] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [bat] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% -o bat.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [bat] Build failed with g++
    exit /b 1
)

echo [bat] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [bat] Build successful: bat.exe
if exist "..\..\bin" (
    copy /y bat.exe "..\..\bin\bat.exe" >nul
    echo [bat] Installed to bin\bat.exe
)
exit /b 0

:clean
echo [bat] Cleaning build artifacts...
del /q bat.exe *.obj *.o *.pdb *.ilk 2>nul
echo [bat] Clean complete.
exit /b 0
