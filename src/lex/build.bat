@echo off
setlocal enabledelayedexpansion

echo [lex] Building lex...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=lex.cpp lex_app.cpp engine.cpp options.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lex] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -Wl,/subsystem:console -o lex.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lex] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lex] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:lex.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [lex] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [lex] Compiling with g++...
    g++ -std=c++17 -O2 -municode %SOURCES% -o lex.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [lex] Build failed with g++
    exit /b 1
)

echo [lex] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [lex] Build successful: lex.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y lex.exe "..\..\bin\lex.exe" >nul
    echo [lex] Installed to bin\lex.exe
)
:done
exit /b 0

:clean
echo [lex] Cleaning build artifacts...
del /q lex.exe *.obj *.o *.pdb *.ilk 2>nul
echo [lex] Clean complete.
exit /b 0
