@echo off
setlocal enabledelayedexpansion

echo [diskinfo] Building diskinfo...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=diskinfo.cpp diskinfo_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS=-lmpr"
set "MSVC_LIBS=mpr.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [diskinfo] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o diskinfo.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [diskinfo] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [diskinfo] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:diskinfo.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [diskinfo] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [diskinfo] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o diskinfo.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [diskinfo] Build failed with g++
    exit /b 1
)

echo [diskinfo] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [diskinfo] Build successful: diskinfo.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y diskinfo.exe "..\..\bin\diskinfo.exe" >nul
    echo [diskinfo] Installed to bin\diskinfo.exe
)
:done
exit /b 0

:clean
echo [diskinfo] Cleaning build artifacts...
del /q diskinfo.exe *.obj *.o *.pdb *.ilk 2>nul
echo [diskinfo] Clean complete.
exit /b 0
