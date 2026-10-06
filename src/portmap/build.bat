@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=portmap.exe"
set "SOURCES=portmap.cpp portmap_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS=ws2_32.lib iphlpapi.lib psapi.lib"

if "%1"=="clean" (
    echo [portmap] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [portmap] Clean complete.
    exit /b 0
)

echo [portmap] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [portmap] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -lws2_32 -liphlpapi -lpsapi -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [portmap] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG %SOURCES% -lws2_32 -liphlpapi -lpsapi -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [portmap] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% %LIBS% /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [portmap] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [portmap] Build failed!
    exit /b 1
)

echo [portmap] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [portmap] Installed to bin\%TARGET%
    )
)
