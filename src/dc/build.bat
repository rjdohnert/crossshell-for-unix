@echo off
setlocal enabledelayedexpansion

echo [dc] Building dc...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=dc.cpp dc_app.cpp calculator.cpp values.cpp big_number.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [dc] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -o dc.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [dc] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [dc] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:dc.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [dc] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [dc] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% -o dc.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [dc] Build failed with g++
    exit /b 1
)

echo [dc] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [dc] Build successful: dc.exe
if exist "..\..\bin" (
    copy /y dc.exe "..\..\bin\dc.exe" >nul
    echo [dc] Installed to bin\dc.exe
)
exit /b 0

:clean
echo [dc] Cleaning build artifacts...
del /q dc.exe *.obj *.o *.pdb *.ilk 2>nul
echo [dc] Clean complete.
exit /b 0
