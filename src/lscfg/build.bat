@echo off
setlocal enabledelayedexpansion

echo [lscfg] Building lscfg...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=lscfg.cpp lscfg_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS=-lwbemuuid -lole32 -loleaut32"
set "MSVC_LIBS=wbemuuid.lib ole32.lib oleaut32.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lscfg] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o lscfg.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lscfg] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lscfg] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:lscfg.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [lscfg] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lscfg] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o lscfg.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lscfg] Build failed with g++
    exit /b 1
)

echo [lscfg] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [lscfg] Build successful: lscfg.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y lscfg.exe "..\..\bin\lscfg.exe" >nul
    echo [lscfg] Installed to bin\lscfg.exe
)
:done
exit /b 0

:clean
echo [lscfg] Cleaning build artifacts...
del /q lscfg.exe *.obj *.o *.pdb *.ilk 2>nul
echo [lscfg] Clean complete.
exit /b 0
