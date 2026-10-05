@echo off
setlocal enabledelayedexpansion

echo [base] Building base...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=base.cpp base_app.cpp codec.cpp engine.cpp options.cpp reporter.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [base] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -o base.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [base] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [base] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:base.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [base] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [base] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% -o base.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [base] Build failed with g++
    exit /b 1
)

echo [base] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [base] Build successful: base.exe
if exist "..\..\bin" (
    copy /y base.exe "..\..\bin\base.exe" >nul
    echo [base] Installed to bin\base.exe
)
exit /b 0

:clean
echo [base] Cleaning build artifacts...
del /q base.exe *.obj *.o *.pdb *.ilk 2>nul
echo [base] Clean complete.
exit /b 0
