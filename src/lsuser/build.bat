@echo off
setlocal enabledelayedexpansion

echo [lsuser] Building lsuser...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=lsuser.cpp lsuser_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS=-lnetapi32 -ladvapi32 -lsecur32"
set "MSVC_LIBS=Netapi32.lib Advapi32.lib Secur32.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsuser] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o lsuser.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lsuser] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsuser] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:lsuser.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [lsuser] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsuser] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o lsuser.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lsuser] Build failed with g++
    exit /b 1
)

echo [lsuser] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [lsuser] Build successful: lsuser.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y lsuser.exe "..\..\bin\lsuser.exe" >nul
    echo [lsuser] Installed to bin\lsuser.exe
)
:done
exit /b 0

:clean
echo [lsuser] Cleaning build artifacts...
del /q lsuser.exe *.obj *.o *.pdb *.ilk 2>nul
echo [lsuser] Clean complete.
exit /b 0
