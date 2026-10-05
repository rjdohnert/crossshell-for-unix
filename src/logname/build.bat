@echo off
setlocal enabledelayedexpansion

echo [logname] Building logname...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=logname.cpp logname_app.cpp engine.cpp options.cpp"
set "LIBS=-ladvapi32"
set "MSVC_LIBS=advapi32.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [logname] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o logname.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [logname] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [logname] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:logname.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [logname] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [logname] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o logname.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [logname] Build failed with g++
    exit /b 1
)

echo [logname] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [logname] Build successful: logname.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y logname.exe "..\..\bin\logname.exe" >nul
    echo [logname] Installed to bin\logname.exe
)
:done
exit /b 0

:clean
echo [logname] Cleaning build artifacts...
del /q logname.exe *.obj *.o *.pdb *.ilk 2>nul
echo [logname] Clean complete.
exit /b 0
