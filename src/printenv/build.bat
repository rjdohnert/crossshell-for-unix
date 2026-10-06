@echo off
setlocal enabledelayedexpansion

set TOOL_NAME=printenv
set SOURCE_FILES=printenv.cpp options.cpp reporter.cpp engine.cpp printenv_app.cpp
set BIN_DIR=..\..\bin

if "%1"=="clean" goto do_clean
if "%1"=="--clean" goto do_clean

:: Detect compiler
set COMPILER=
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    set COMPILER=clang
    goto do_build
)
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    set COMPILER=cl
    goto do_build
)
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    set COMPILER=gcc
    goto do_build
)

echo Error: No suitable C++ compiler found (clang++, cl, or g++).
exit /b 1

:do_build
echo Building %TOOL_NAME% with %COMPILER%...

if "%COMPILER%"=="clang" (
    clang++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -Wl,/subsystem:console -o %TOOL_NAME%.exe %SOURCE_FILES%
)
if "%COMPILER%"=="cl" (
    cl /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /Fe:%TOOL_NAME%.exe %SOURCE_FILES%
)
if "%COMPILER%"=="gcc" (
    g++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -municode -o %TOOL_NAME%.exe %SOURCE_FILES%
)

if %ERRORLEVEL% neq 0 (
    echo Build failed.
    exit /b %ERRORLEVEL%
)

echo Build succeeded: %TOOL_NAME%.exe

if "%1"=="--no-install" goto done
if "%2"=="--no-install" goto done

if exist %BIN_DIR% (
    copy /y %TOOL_NAME%.exe %BIN_DIR%\%TOOL_NAME%.exe >nul
    echo Installed to %BIN_DIR%\%TOOL_NAME%.exe
)

goto done

:do_clean
echo Cleaning build artifacts for %TOOL_NAME%...
del /q *.obj *.o *.exe *.pdb *.ilk 2>nul
echo Clean completed.

:done
endlocal
