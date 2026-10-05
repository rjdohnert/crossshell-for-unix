@echo off
setlocal enabledelayedexpansion

echo [chmod] Building chmod...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=chmod.cpp chmod_app.cpp acl_manager.cpp mode_parser.cpp options.cpp reporter.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chmod] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -ladvapi32 -o chmod.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [chmod] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chmod] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% advapi32.lib /Fe:chmod.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [chmod] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chmod] Compiling with g++...
    g++ -std=c++17 -O2 -municode %SOURCES% -ladvapi32 -o chmod.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [chmod] Build failed with g++
    exit /b 1
)

echo [chmod] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [chmod] Build successful: chmod.exe
if exist "..\..\bin" (
    copy /y chmod.exe "..\..\bin\chmod.exe" >nul
    echo [chmod] Installed to bin\chmod.exe
)
exit /b 0

:clean
echo [chmod] Cleaning build artifacts...
del /q chmod.exe *.obj *.o *.pdb *.ilk 2>nul
echo [chmod] Clean complete.
exit /b 0
