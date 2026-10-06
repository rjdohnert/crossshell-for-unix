@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=pbpaste.exe"
set "SOURCES=pbpaste.cpp pbpaste_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [pbpaste] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [pbpaste] Clean complete.
    exit /b 0
)

echo [pbpaste] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [pbpaste] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -luser32 -lshell32 -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [pbpaste] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% -luser32 -lshell32 -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [pbpaste] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% user32.lib shell32.lib /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [pbpaste] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [pbpaste] Build failed!
    exit /b 1
)

echo [pbpaste] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [pbpaste] Installed to bin\%TARGET%
    )
)
