@echo off
setlocal enabledelayedexpansion

echo [chattr] Building chattr...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=chattr.cpp chattr_app.cpp engine.cpp options.cpp reporter.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chattr] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -o chattr.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [chattr] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chattr] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:chattr.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [chattr] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chattr] Compiling with g++...
    g++ -std=c++17 -O2 -municode %SOURCES% -o chattr.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [chattr] Build failed with g++
    exit /b 1
)

echo [chattr] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [chattr] Build successful: chattr.exe
if exist "..\..\bin" (
    copy /y chattr.exe "..\..\bin\chattr.exe" >nul
    echo [chattr] Installed to bin\chattr.exe
)
exit /b 0

:clean
echo [chattr] Cleaning build artifacts...
del /q chattr.exe *.obj *.o *.pdb *.ilk 2>nul
echo [chattr] Clean complete.
exit /b 0
