@echo off
setlocal enabledelayedexpansion

echo [cut] Building cut...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=cut.cpp cut_app.cpp range_parser.cpp stream_processor.cpp options.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [cut] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -o cut.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [cut] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [cut] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:cut.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [cut] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [cut] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% -o cut.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [cut] Build failed with g++
    exit /b 1
)

echo [cut] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [cut] Build successful: cut.exe
if exist "..\..\bin" (
    copy /y cut.exe "..\..\bin\cut.exe" >nul
    echo [cut] Installed to bin\cut.exe
)
exit /b 0

:clean
echo [cut] Cleaning build artifacts...
del /q cut.exe *.obj *.o *.pdb *.ilk 2>nul
echo [cut] Clean complete.
exit /b 0
