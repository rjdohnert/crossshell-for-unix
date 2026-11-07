@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "TARGET=diff"
set "BIN_DIR=%SCRIPT_DIR%..\..\bin"
set "SRC=%SCRIPT_DIR%diff.c"

if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"

where clang >nul 2>nul
if %errorlevel% equ 0 (
    echo Building %TARGET% with clang...
    clang -O2 "%SRC%" -o "%BIN_DIR%\%TARGET%.exe"
    if %errorlevel% equ 0 (
        echo Build successful: %BIN_DIR%\%TARGET%.exe
        exit /b 0
    )
)

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo Building %TARGET% with clang++...
    clang++ -O2 "%SRC%" -o "%BIN_DIR%\%TARGET%.exe"
    if %errorlevel% equ 0 (
        echo Build successful: %BIN_DIR%\%TARGET%.exe
        exit /b 0
    )
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo Building %TARGET% with MSVC cl...
    cl /O2 /Fe:"%BIN_DIR%\%TARGET%.exe" "%SRC%"
    if %errorlevel% equ 0 (
        echo Build successful: %BIN_DIR%\%TARGET%.exe
        if exist "%SCRIPT_DIR%*.obj" del "%SCRIPT_DIR%*.obj"
        exit /b 0
    )
)

where gcc >nul 2>nul
if %errorlevel% equ 0 (
    echo Building %TARGET% with gcc...
    gcc -O2 "%SRC%" -o "%BIN_DIR%\%TARGET%.exe"
    if %errorlevel% equ 0 (
        echo Build successful: %BIN_DIR%\%TARGET%.exe
        exit /b 0
    )
)

echo Error: No suitable C compiler found (clang, cl, or gcc).
exit /b 1
