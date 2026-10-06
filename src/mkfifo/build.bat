@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=mkfifo.exe"
set "SOURCES=mkfifo.cpp mkfifo_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [mkfifo] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [mkfifo] Clean complete.
    exit /b 0
)

echo [mkfifo] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [mkfifo] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -ladvapi32 -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [mkfifo] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -ladvapi32 -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [mkfifo] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% advapi32.lib /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [mkfifo] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [mkfifo] Build failed!
    exit /b 1
)

echo [mkfifo] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [mkfifo] Installed to bin\%TARGET%
    )
)
