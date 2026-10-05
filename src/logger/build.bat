@echo off
setlocal enabledelayedexpansion

echo [logger] Building logger...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=logger.cpp logger_app.cpp engine.cpp options.cpp"
set "LIBS=-ladvapi32"
set "MSVC_LIBS=Advapi32.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [logger] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o logger.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [logger] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [logger] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:logger.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [logger] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [logger] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o logger.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [logger] Build failed with g++
    exit /b 1
)

echo [logger] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [logger] Build successful: logger.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y logger.exe "..\..\bin\logger.exe" >nul
    echo [logger] Installed to bin\logger.exe
)
:done
exit /b 0

:clean
echo [logger] Cleaning build artifacts...
del /q logger.exe *.obj *.o *.pdb *.ilk 2>nul
echo [logger] Clean complete.
exit /b 0
