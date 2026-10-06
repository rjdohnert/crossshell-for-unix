@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=mount.exe"
set "SOURCES=mount.cpp mount_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [mount] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [mount] Clean complete.
    exit /b 0
)

echo [mount] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [mount] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -lmpr -lvirtdisk -ladvapi32 -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [mount] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -lmpr -lvirtdisk -ladvapi32 -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [mount] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% mpr.lib virtdisk.lib advapi32.lib /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [mount] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [mount] Build failed!
    exit /b 1
)

echo [mount] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [mount] Installed to bin\%TARGET%
    )
)
