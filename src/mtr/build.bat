@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=mtr.exe"
set "SOURCES=mtr.cpp mtr_app.cpp engine.cpp reporter.cpp options.cpp"

if "%1"=="clean" (
    echo [mtr] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [mtr] Clean complete.
    exit /b 0
)

echo [mtr] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [mtr] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -lws2_32 -liphlpapi -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [mtr] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -lws2_32 -liphlpapi -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [mtr] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% ws2_32.lib iphlpapi.lib /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [mtr] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [mtr] Build failed!
    exit /b 1
)

echo [mtr] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [mtr] Installed to bin\%TARGET%
    )
)
