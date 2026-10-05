@echo off
setlocal enabledelayedexpansion

echo [lsbt] Building lsbt...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=lsbt.cpp lsbt_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS=-lbthprops"
set "MSVC_LIBS=bthprops.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsbt] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o lsbt.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lsbt] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsbt] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:lsbt.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [lsbt] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsbt] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o lsbt.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lsbt] Build failed with g++
    exit /b 1
)

echo [lsbt] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [lsbt] Build successful: lsbt.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y lsbt.exe "..\..\bin\lsbt.exe" >nul
    echo [lsbt] Installed to bin\lsbt.exe
)
:done
exit /b 0

:clean
echo [lsbt] Cleaning build artifacts...
del /q lsbt.exe *.obj *.o *.pdb *.ilk 2>nul
echo [lsbt] Clean complete.
exit /b 0
