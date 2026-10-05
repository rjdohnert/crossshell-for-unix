@echo off
setlocal enabledelayedexpansion

echo [ls] Building ls...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=ls.cpp ls_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS=-ladvapi32"
set "MSVC_LIBS=advapi32.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [ls] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o ls.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [ls] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [ls] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:ls.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [ls] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [ls] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o ls.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [ls] Build failed with g++
    exit /b 1
)

echo [ls] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [ls] Build successful: ls.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y ls.exe "..\..\bin\ls.exe" >nul
    echo [ls] Installed to bin\ls.exe
)
:done
exit /b 0

:clean
echo [ls] Cleaning build artifacts...
del /q ls.exe *.obj *.o *.pdb *.ilk 2>nul
echo [ls] Clean complete.
exit /b 0
