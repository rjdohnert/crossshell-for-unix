@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=man.exe"
set "SOURCES=man.cpp man_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [man] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [man] Clean complete.
    exit /b 0
)

echo [man] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [man] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [man] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [man] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [man] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [man] Build failed!
    exit /b 1
)

echo [man] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [man] Installed to bin\%TARGET%
    )
)
