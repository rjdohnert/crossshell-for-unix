@echo off
setlocal enabledelayedexpansion

echo [chown] Building chown...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=chown.cpp chown_app.cpp ownership.cpp options.cpp reporter.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chown] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -ladvapi32 -o chown.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [chown] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chown] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% advapi32.lib /Fe:chown.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [chown] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chown] Compiling with g++...
    g++ -std=c++17 -O2 -municode %SOURCES% -ladvapi32 -o chown.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [chown] Build failed with g++
    exit /b 1
)

echo [chown] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [chown] Build successful: chown.exe
if exist "..\..\bin" (
    copy /y chown.exe "..\..\bin\chown.exe" >nul
    echo [chown] Installed to bin\chown.exe
)
exit /b 0

:clean
echo [chown] Cleaning build artifacts...
del /q chown.exe *.obj *.o *.pdb *.ilk 2>nul
echo [chown] Clean complete.
exit /b 0
