@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=patch.exe"
set "SOURCES=patch.cpp patch_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [patch] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [patch] Clean complete.
    exit /b 0
)

echo [patch] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [patch] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [patch] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [patch] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [patch] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [patch] Build failed!
    exit /b 1
)

echo [patch] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [patch] Installed to bin\%TARGET%
    )
)
