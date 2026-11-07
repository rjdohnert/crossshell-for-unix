@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
set "TARGET=diff3"
set "BIN_DIR=%SCRIPT_DIR%..\..\bin"

if exist "%SCRIPT_DIR%diff3.c" (
    set "SRC=%SCRIPT_DIR%diff3.c"
) else if exist "%SCRIPT_DIR%diff3_app.cpp" (
    set "SRC=%SCRIPT_DIR%diff3.cpp %SCRIPT_DIR%diff3_app.cpp %SCRIPT_DIR%arg_parser.cpp %SCRIPT_DIR%formatter.cpp %SCRIPT_DIR%diff3_engine.cpp %SCRIPT_DIR%myers_diff.cpp %SCRIPT_DIR%line_reader.cpp"
) else (
    set "SRC=%SCRIPT_DIR%diff3.cpp"
)

if not exist "%BIN_DIR%" mkdir "%BIN_DIR%"

where clang >nul 2>nul
if %errorlevel% equ 0 (
    echo Building %TARGET% with clang...
    clang -O2 %SRC% -o "%BIN_DIR%\%TARGET%.exe"
    if %errorlevel% equ 0 (
        echo Build successful: %BIN_DIR%\%TARGET%.exe
        exit /b 0
    )
)

where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo Building %TARGET% with clang++...
    clang++ -std=c++17 -O2 %SRC% -o "%BIN_DIR%\%TARGET%.exe"
    if %errorlevel% equ 0 (
        echo Build successful: %BIN_DIR%\%TARGET%.exe
        exit /b 0
    )
)

where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo Building %TARGET% with MSVC cl...
    cl /std:c++17 /O2 /EHsc /Fe:"%BIN_DIR%\%TARGET%.exe" %SRC%
    if %errorlevel% equ 0 (
        echo Build successful: %BIN_DIR%\%TARGET%.exe
        if exist "%SCRIPT_DIR%*.obj" del "%SCRIPT_DIR%*.obj"
        exit /b 0
    )
)

where gcc >nul 2>nul
if %errorlevel% equ 0 (
    echo Building %TARGET% with gcc...
    gcc -O2 %SRC% -o "%BIN_DIR%\%TARGET%.exe"
    if %errorlevel% equ 0 (
        echo Build successful: %BIN_DIR%\%TARGET%.exe
        exit /b 0
    )
)

where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo Building %TARGET% with g++...
    g++ -std=c++17 -O2 %SRC% -o "%BIN_DIR%\%TARGET%.exe"
    if %errorlevel% equ 0 (
        echo Build successful: %BIN_DIR%\%TARGET%.exe
        exit /b 0
    )
)

echo Error: No suitable compiler found (clang, clang++, cl, gcc, or g++).
exit /b 1
