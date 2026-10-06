@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=newshell.exe"
set "SOURCES=newshell.cpp newshell_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [newshell] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [newshell] Clean complete.
    exit /b 0
)

echo [newshell] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [newshell] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -lshell32 -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [newshell] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -lshell32 -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [newshell] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% shell32.lib /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [newshell] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [newshell] Build failed!
    exit /b 1
)

echo [newshell] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [newshell] Installed to bin\%TARGET%
    )
)
