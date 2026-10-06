@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=pinky.exe"
set "SOURCES=pinky.cpp pinky_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS=wtsapi32.lib netapi32.lib ws2_32.lib"

if "%1"=="clean" (
    echo [pinky] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [pinky] Clean complete.
    exit /b 0
)

echo [pinky] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [pinky] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -lwtsapi32 -lnetapi32 -lws2_32 -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [pinky] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -lwtsapi32 -lnetapi32 -lws2_32 -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [pinky] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% %LIBS% /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [pinky] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [pinky] Build failed!
    exit /b 1
)

echo [pinky] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [pinky] Installed to bin\%TARGET%
    )
)
