@echo off
setlocal enabledelayedexpansion

echo [ltrace] Building ltrace...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=ltrace.cpp ltrace_app.cpp engine.cpp options.cpp"
set "LIBS=-lpsapi"
set "MSVC_LIBS=psapi.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [ltrace] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o ltrace.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [ltrace] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [ltrace] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:ltrace.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [ltrace] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [ltrace] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o ltrace.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [ltrace] Build failed with g++
    exit /b 1
)

echo [ltrace] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [ltrace] Build successful: ltrace.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y ltrace.exe "..\..\bin\ltrace.exe" >nul
    echo [ltrace] Installed to bin\ltrace.exe
)
:done
exit /b 0

:clean
echo [ltrace] Cleaning build artifacts...
del /q ltrace.exe *.obj *.o *.pdb *.ilk 2>nul
echo [ltrace] Clean complete.
exit /b 0
