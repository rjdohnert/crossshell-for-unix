@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=makedepend.exe"
set "SOURCES=makedepend.cpp makedepend_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [makedepend] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [makedepend] Clean complete.
    exit /b 0
)

echo [makedepend] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [makedepend] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [makedepend] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [makedepend] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [makedepend] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [makedepend] Build failed!
    exit /b 1
)

echo [makedepend] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [makedepend] Installed to bin\%TARGET%
    )
)
