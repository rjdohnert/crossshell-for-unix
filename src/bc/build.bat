@echo off
setlocal enabledelayedexpansion

echo [bc] Building bc...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=bc.cpp bc_app.cpp engine.cpp options.cpp parser.cpp reporter.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [bc] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -o bc.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [bc] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [bc] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:bc.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [bc] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [bc] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% -o bc.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [bc] Build failed with g++
    exit /b 1
)

echo [bc] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [bc] Build successful: bc.exe
if exist "..\..\bin" (
    copy /y bc.exe "..\..\bin\bc.exe" >nul
    echo [bc] Installed to bin\bc.exe
)
exit /b 0

:clean
echo [bc] Cleaning build artifacts...
del /q bc.exe *.obj *.o *.pdb *.ilk 2>nul
echo [bc] Clean complete.
exit /b 0
