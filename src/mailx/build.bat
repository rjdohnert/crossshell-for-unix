@echo off
setlocal enabledelayedexpansion

echo [mailx] Building mailx...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=mailx.cpp mailx_app.cpp engine.cpp options.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [mailx] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -Wl,/subsystem:console -o mailx.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [mailx] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [mailx] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:mailx.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [mailx] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [mailx] Compiling with g++...
    g++ -std=c++17 -O2 -municode %SOURCES% -o mailx.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [mailx] Build failed with g++
    exit /b 1
)

echo [mailx] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [mailx] Build successful: mailx.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y mailx.exe "..\..\bin\mailx.exe" >nul
    echo [mailx] Installed to bin\mailx.exe
)
:done
exit /b 0

:clean
echo [mailx] Cleaning build artifacts...
del /q mailx.exe *.obj *.o *.pdb *.ilk 2>nul
echo [mailx] Clean complete.
exit /b 0
