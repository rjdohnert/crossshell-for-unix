@echo off
setlocal enabledelayedexpansion

echo [chcon] Building chcon...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=chcon.cpp chcon_app.cpp security.cpp options.cpp reporter.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chcon] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -ladvapi32 -o chcon.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [chcon] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chcon] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% advapi32.lib /Fe:chcon.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [chcon] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chcon] Compiling with g++...
    g++ -std=c++17 -O2 -municode %SOURCES% -ladvapi32 -o chcon.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [chcon] Build failed with g++
    exit /b 1
)

echo [chcon] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [chcon] Build successful: chcon.exe
if exist "..\..\bin" (
    copy /y chcon.exe "..\..\bin\chcon.exe" >nul
    echo [chcon] Installed to bin\chcon.exe
)
exit /b 0

:clean
echo [chcon] Cleaning build artifacts...
del /q chcon.exe *.obj *.o *.pdb *.ilk 2>nul
echo [chcon] Clean complete.
exit /b 0
