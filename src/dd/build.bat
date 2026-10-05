@echo off
setlocal enabledelayedexpansion

echo [dd] Building dd...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=dd.cpp dd_app.cpp block_copier.cpp data_transformer.cpp size_parser.cpp options.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [dd] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -o dd.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [dd] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [dd] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:dd.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [dd] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [dd] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% -o dd.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [dd] Build failed with g++
    exit /b 1
)

echo [dd] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [dd] Build successful: dd.exe
if exist "..\..\bin" (
    copy /y dd.exe "..\..\bin\dd.exe" >nul
    echo [dd] Installed to bin\dd.exe
)
exit /b 0

:clean
echo [dd] Cleaning build artifacts...
del /q dd.exe *.obj *.o *.pdb *.ilk 2>nul
echo [dd] Clean complete.
exit /b 0
