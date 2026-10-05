@echo off
setlocal enabledelayedexpansion

echo [logout] Building logout...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=logout.cpp logout_app.cpp engine.cpp options.cpp"
set "LIBS=-luser32"
set "MSVC_LIBS=user32.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [logout] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o logout.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [logout] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [logout] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:logout.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [logout] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [logout] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o logout.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [logout] Build failed with g++
    exit /b 1
)

echo [logout] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [logout] Build successful: logout.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y logout.exe "..\..\bin\logout.exe" >nul
    echo [logout] Installed to bin\logout.exe
)
:done
exit /b 0

:clean
echo [logout] Cleaning build artifacts...
del /q logout.exe *.obj *.o *.pdb *.ilk 2>nul
echo [logout] Clean complete.
exit /b 0
