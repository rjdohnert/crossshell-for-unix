@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=paste.exe"
set "SOURCES=paste.cpp paste_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [paste] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [paste] Clean complete.
    exit /b 0
)

echo [paste] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [paste] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [paste] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [paste] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [paste] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [paste] Build failed!
    exit /b 1
)

echo [paste] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [paste] Installed to bin\%TARGET%
    )
)
