@echo off
setlocal enabledelayedexpansion

echo [arch] Building arch...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=arch.cpp arch_app.cpp engine.cpp options.cpp reporter.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [arch] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -o arch.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [arch] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [arch] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:arch.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [arch] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [arch] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% -o arch.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [arch] Build failed with g++
    exit /b 1
)

echo [arch] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [arch] Build successful: arch.exe
if exist "..\..\bin" (
    copy /y arch.exe "..\..\bin\arch.exe" >nul
    echo [arch] Installed to bin\arch.exe
)
exit /b 0

:clean
echo [arch] Cleaning build artifacts...
del /q arch.exe *.obj *.o *.pdb *.ilk 2>nul
echo [arch] Clean complete.
exit /b 0
