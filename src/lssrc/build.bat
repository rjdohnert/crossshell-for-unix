@echo off
setlocal enabledelayedexpansion

echo [lssrc] Building lssrc...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=lssrc.cpp lssrc_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS=-ladvapi32"
set "MSVC_LIBS=advapi32.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lssrc] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o lssrc.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lssrc] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lssrc] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:lssrc.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [lssrc] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lssrc] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o lssrc.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lssrc] Build failed with g++
    exit /b 1
)

echo [lssrc] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [lssrc] Build successful: lssrc.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y lssrc.exe "..\..\bin\lssrc.exe" >nul
    echo [lssrc] Installed to bin\lssrc.exe
)
:done
exit /b 0

:clean
echo [lssrc] Cleaning build artifacts...
del /q lssrc.exe *.obj *.o *.pdb *.ilk 2>nul
echo [lssrc] Clean complete.
exit /b 0
