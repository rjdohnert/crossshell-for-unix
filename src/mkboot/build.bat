@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=mkboot.exe"
set "SOURCES=mkboot.cpp mkboot_app.cpp engine.cpp options.cpp"

if "%1"=="clean" (
    echo [mkboot] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [mkboot] Clean complete.
    exit /b 0
)

echo [mkboot] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [mkboot] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -lole32 -loleaut32 -lshlwapi -ladvapi32 -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [mkboot] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -lole32 -loleaut32 -lshlwapi -ladvapi32 -luuid -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [mkboot] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% ole32.lib oleaut32.lib shlwapi.lib advapi32.lib /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [mkboot] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [mkboot] Build failed!
    exit /b 1
)

echo [mkboot] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [mkboot] Installed to bin\%TARGET%
    )
)
