@echo off
setlocal enabledelayedexpansion

echo [lsattr] Building lsattr...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=lsattr.cpp lsattr_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS="
set "MSVC_LIBS="

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsattr] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -Wl,/subsystem:console -o lsattr.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lsattr] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsattr] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:lsattr.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [lsattr] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lsattr] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% -o lsattr.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lsattr] Build failed with g++
    exit /b 1
)

echo [lsattr] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [lsattr] Build successful: lsattr.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y lsattr.exe "..\..\bin\lsattr.exe" >nul
    echo [lsattr] Installed to bin\lsattr.exe
)
:done
exit /b 0

:clean
echo [lsattr] Cleaning build artifacts...
del /q lsattr.exe *.obj *.o *.pdb *.ilk 2>nul
echo [lsattr] Clean complete.
exit /b 0
