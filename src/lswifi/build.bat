@echo off
setlocal enabledelayedexpansion

echo [lswifi] Building lswifi...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=lswifi.cpp lswifi_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS=-lwlanapi -lole32"
set "MSVC_LIBS=wlanapi.lib ole32.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lswifi] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o lswifi.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lswifi] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lswifi] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:lswifi.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [lswifi] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lswifi] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o lswifi.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lswifi] Build failed with g++
    exit /b 1
)

echo [lswifi] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [lswifi] Build successful: lswifi.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y lswifi.exe "..\..\bin\lswifi.exe" >nul
    echo [lswifi] Installed to bin\lswifi.exe
)
:done
exit /b 0

:clean
echo [lswifi] Cleaning build artifacts...
del /q lswifi.exe *.obj *.o *.pdb *.ilk 2>nul
echo [lswifi] Clean complete.
exit /b 0
