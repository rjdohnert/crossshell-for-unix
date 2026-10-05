@echo off
setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%"

if /i "%1"=="clean" goto do_clean

echo Building dos2unix and unix2dos...

set SOURCES=dos2unix.cpp app.cpp converter.cpp options.cpp reporter.cpp timestamp_helper.cpp
set OUT_EXE=dos2unix.exe
set ALIAS_EXE=unix2dos.exe

REM 1. Try MSVC cl.exe
where cl >nul 2>nul
if %errorlevel% equ 0 (
    echo Using MSVC compiler (cl.exe)...
    cl /nologo /std:c++17 /EHsc /W4 /O2 /permissive- /utf-8 /Fe:%OUT_EXE% %SOURCES% Shell32.lib
    if %errorlevel% neq 0 (
        echo Build failed!
        exit /b %errorlevel%
    )
    goto post_build
)

REM 2. Try CrossShell vcc
where vcc >nul 2>nul
if %errorlevel% equ 0 (
    echo Using vcc compiler...
    vcc -std=c++17 -O2 -o %OUT_EXE% %SOURCES% -lshell32
    if %errorlevel% neq 0 (
        echo Build failed!
        exit /b %errorlevel%
    )
    goto post_build
)

REM 3. Try clang++
where clang++ >nul 2>nul
if %errorlevel% equ 0 (
    echo Using clang++...
    clang++ -std=c++17 -O2 -municode -o %OUT_EXE% %SOURCES% -lshell32
    if %errorlevel% neq 0 (
        echo Build failed!
        exit /b %errorlevel%
    )
    goto post_build
)

REM 4. Try g++
where g++ >nul 2>nul
if %errorlevel% equ 0 (
    echo Using g++...
    g++ -std=c++17 -O2 -municode -o %OUT_EXE% %SOURCES% -lshell32
    if %errorlevel% neq 0 (
        echo Build failed!
        exit /b %errorlevel%
    )
    goto post_build
)

echo ERROR: No supported C++ compiler found (cl.exe, vcc, clang++, or g++).
echo Please run this script from a Visual Studio Developer Command Prompt or ensure a compiler is in your PATH.
exit /b 1

:post_build
copy /y %OUT_EXE% %ALIAS_EXE% >nul
if exist "..\..\bin" (
    copy /y %OUT_EXE% "..\..\bin\" >nul
    copy /y %ALIAS_EXE% "..\..\bin\" >nul
    echo Installed %OUT_EXE% and %ALIAS_EXE% to ..\..\bin\
)
echo Build succeeded: %OUT_EXE% and %ALIAS_EXE% created.
exit /b 0

:do_clean
echo Cleaning build artifacts...
del /q /f *.obj *.o *.exe *.pdb *.ilk >nul 2>nul
echo Clean complete.
exit /b 0
