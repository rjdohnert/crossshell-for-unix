@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=mv.exe"
set "SOURCES=mv.cpp mv_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [mv] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [mv] Clean complete.
    exit /b 0
)

echo [mv] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [mv] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [mv] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [mv] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [mv] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [mv] Build failed!
    exit /b 1
)

echo [mv] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [mv] Installed to bin\%TARGET%
    )
)
