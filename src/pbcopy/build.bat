@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=pbcopy.exe"
set "SOURCES=pbcopy.cpp pbcopy_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [pbcopy] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [pbcopy] Clean complete.
    exit /b 0
)

echo [pbcopy] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [pbcopy] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -luser32 -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [pbcopy] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -luser32 -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [pbcopy] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% user32.lib /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [pbcopy] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [pbcopy] Build failed!
    exit /b 1
)

echo [pbcopy] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [pbcopy] Installed to bin\%TARGET%
    )
)
