@echo off
setlocal enabledelayedexpansion

echo [netconf] Building netconf...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=netconf.c"
set "LIBS=iphlpapi.lib ws2_32.lib"
set "GCC_LIBS=-liphlpapi -lws2_32"

if "%1"=="clean" goto :clean

:: Check for clang
where clang >nul 2>nul
if !ERRORLEVEL! equ 0 (
    echo [netconf] Compiling with clang...
    clang -O2 %SOURCES% %GCC_LIBS% -o netconf.exe
    if !ERRORLEVEL! equ 0 goto :success
    echo [netconf] Build failed with clang
    exit /b 1
)

:: Check for clang++
where clang++ >nul 2>nul
if !ERRORLEVEL! equ 0 (
    echo [netconf] Compiling with clang++...
    clang++ -O2 %SOURCES% %GCC_LIBS% -o netconf.exe
    if !ERRORLEVEL! equ 0 goto :success
    echo [netconf] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if !ERRORLEVEL! equ 0 (
    echo [netconf] Compiling with cl.exe...
    cl /nologo /O2 %SOURCES% %LIBS% /Fe:netconf.exe
    if !ERRORLEVEL! equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [netconf] Build failed with cl.exe
    exit /b 1
)

:: Check for gcc
where gcc >nul 2>nul
if !ERRORLEVEL! equ 0 (
    echo [netconf] Compiling with gcc...
    gcc -O2 %SOURCES% %GCC_LIBS% -o netconf.exe
    if !ERRORLEVEL! equ 0 goto :success
    echo [netconf] Build failed with gcc
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if !ERRORLEVEL! equ 0 (
    echo [netconf] Compiling with g++...
    g++ -O2 %SOURCES% %GCC_LIBS% -o netconf.exe
    if !ERRORLEVEL! equ 0 goto :success
    echo [netconf] Build failed with g++
    exit /b 1
)

echo [netconf] Error: No supported C/C++ compiler found (clang, clang++, cl.exe, gcc, g++).
exit /b 1

:success
echo [netconf] Build successful: netconf.exe
if exist "..\..\bin" (
    copy /y netconf.exe "..\..\bin\netconf.exe" >nul
    echo [netconf] Installed to bin\netconf.exe
)
exit /b 0

:clean
echo [netconf] Cleaning build artifacts...
del /q netconf.exe *.obj *.o *.pdb *.ilk 2>nul
echo [netconf] Clean complete.
exit /b 0
