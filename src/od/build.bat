@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=od.exe"
set "SOURCES=od.cpp od_app.cpp engine.cpp options.cpp reporter.cpp"

if "%1"=="clean" (
    echo [od] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [od] Clean complete.
    exit /b 0
)

echo [od] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [od] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [od] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [od] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [od] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [od] Build failed!
    exit /b 1
)

echo [od] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [od] Installed to bin\%TARGET%
    )
)
