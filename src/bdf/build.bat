@echo off
setlocal enabledelayedexpansion

echo [bdf] Building bdf...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=bdf.cpp bdf_app.cpp formatter.cpp inspector.cpp options.cpp pipeline.cpp table.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [bdf] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -o bdf.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [bdf] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [bdf] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:bdf.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [bdf] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [bdf] Compiling with g++...
    g++ -std=c++17 -O2 -municode %SOURCES% -o bdf.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [bdf] Build failed with g++
    exit /b 1
)

echo [bdf] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [bdf] Build successful: bdf.exe
if exist "..\..\bin" (
    copy /y bdf.exe "..\..\bin\bdf.exe" >nul
    echo [bdf] Installed to bin\bdf.exe
)
exit /b 0

:clean
echo [bdf] Cleaning build artifacts...
del /q bdf.exe *.obj *.o *.pdb *.ilk 2>nul
echo [bdf] Clean complete.
exit /b 0
