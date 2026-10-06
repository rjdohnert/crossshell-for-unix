@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

set "TARGET=nfsctl.exe"
set "SOURCES=nfsctl.cpp nfsctl_app.cpp controllers.cpp engine.cpp reporter.cpp options.cpp"

if "%1"=="clean" (
    echo [nfsctl] Cleaning build artifacts...
    del /q /f %TARGET% *.obj *.o *.pdb 2>nul
    echo [nfsctl] Clean complete.
    exit /b 0
)

echo [nfsctl] Building %TARGET%...

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [nfsctl] Compiling with clang++...
    clang++ -std=c++17 -O2 -DNDEBUG %SOURCES% -Wl,/subsystem:console -lmpr -lws2_32 -ladvapi32 -lnetapi32 -o %TARGET%
    goto :done
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo [nfsctl] Compiling with g++...
    g++ -std=c++17 -O2 -DNDEBUG -municode %SOURCES% -lmpr -lws2_32 -ladvapi32 -lnetapi32 -o %TARGET%
    goto :done
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo [nfsctl] Compiling with cl...
    cl /nologo /EHsc /std:c++17 /O2 /DNDEBUG %SOURCES% mpr.lib ws2_32.lib advapi32.lib netapi32.lib /Fe:%TARGET%
    del /q /f *.obj 2>nul
    goto :done
)

echo [nfsctl] Error: No C++ compiler found (clang++, g++, cl.exe).
exit /b 1

:done
if not exist %TARGET% (
    echo [nfsctl] Build failed!
    exit /b 1
)

echo [nfsctl] Build successful: %TARGET%

if not "%1"=="noinstall" (
    if exist "..\..\bin" (
        copy /y %TARGET% "..\..\bin\%TARGET%" >nul
        echo [nfsctl] Installed to bin\%TARGET%
    )
)
