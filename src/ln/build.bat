@echo off
setlocal enabledelayedexpansion

echo [ln] Building ln...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=ln.cpp ln_app.cpp engine.cpp options.cpp"
set "LIBS=-ladvapi32"
set "MSVC_LIBS=advapi32.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [ln] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o ln.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [ln] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [ln] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:ln.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [ln] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [ln] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o ln.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [ln] Build failed with g++
    exit /b 1
)

echo [ln] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [ln] Build successful: ln.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y ln.exe "..\..\bin\ln.exe" >nul
    echo [ln] Installed to bin\ln.exe
)
:done
exit /b 0

:clean
echo [ln] Cleaning build artifacts...
del /q ln.exe *.obj *.o *.pdb *.ilk 2>nul
echo [ln] Clean complete.
exit /b 0
