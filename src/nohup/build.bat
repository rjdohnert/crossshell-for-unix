@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=nohup.exe"
set "SOURCES=nohup.cpp nohup_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [nohup] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [nohup] Clean complete.
    exit /b 0
)

echo [nohup] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [nohup] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [nohup] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [nohup] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [nohup] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [nohup] Build failed!
    exit /b 1
)

echo [nohup] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [nohup] Installed to bin\%TARGET%
    )
)
