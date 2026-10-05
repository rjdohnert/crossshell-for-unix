@echo off
setlocal enabledelayedexpansion

echo [chgrp] Building chgrp...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=chgrp.cpp chgrp_app.cpp group_manager.cpp options.cpp reporter.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chgrp] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -ladvapi32 -o chgrp.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [chgrp] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chgrp] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% advapi32.lib /Fe:chgrp.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [chgrp] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [chgrp] Compiling with g++...
    g++ -std=c++17 -O2 -municode %SOURCES% -ladvapi32 -o chgrp.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [chgrp] Build failed with g++
    exit /b 1
)

echo [chgrp] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [chgrp] Build successful: chgrp.exe
if exist "..\..\bin" (
    copy /y chgrp.exe "..\..\bin\chgrp.exe" >nul
    echo [chgrp] Installed to bin\chgrp.exe
)
exit /b 0

:clean
echo [chgrp] Cleaning build artifacts...
del /q chgrp.exe *.obj *.o *.pdb *.ilk 2>nul
echo [chgrp] Clean complete.
exit /b 0
