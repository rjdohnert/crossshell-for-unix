@echo off
setlocal enabledelayedexpansion

echo [lspci] Building lspci...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=lspci.cpp lspci_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS=-lsetupapi"
set "MSVC_LIBS=setupapi.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lspci] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o lspci.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lspci] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lspci] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:lspci.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [lspci] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lspci] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o lspci.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lspci] Build failed with g++
    exit /b 1
)

echo [lspci] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [lspci] Build successful: lspci.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y lspci.exe "..\..\bin\lspci.exe" >nul
    echo [lspci] Installed to bin\lspci.exe
)
:done
exit /b 0

:clean
echo [lspci] Cleaning build artifacts...
del /q lspci.exe *.obj *.o *.pdb *.ilk 2>nul
echo [lspci] Clean complete.
exit /b 0
