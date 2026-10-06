@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=nproc.exe"
set "SOURCES=nproc.cpp nproc_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [nproc] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [nproc] Clean complete.
    exit /b 0
)

echo [nproc] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [nproc] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [nproc] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [nproc] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [nproc] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [nproc] Build failed!
    exit /b 1
)

echo [nproc] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [nproc] Installed to bin\%TARGET%
    )
)
