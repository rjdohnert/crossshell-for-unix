@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=pkill.exe"
set "SOURCES=pkill.cpp pkill_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [pkill] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [pkill] Clean complete.
    exit /b 0
)

echo [pkill] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [pkill] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [pkill] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [pkill] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [pkill] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [pkill] Build failed!
    exit /b 1
)

echo [pkill] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [pkill] Installed to bin\%TARGET%
    )
)
