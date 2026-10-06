@echo off
setlocal enabledelayedexpansion

echo [machinfo] Building machinfo...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=machinfo.cpp machinfo_app.cpp engine.cpp options.cpp reporter.cpp"
set "LIBS=-lwbemuuid -lole32 -loleaut32 -ltbs -ladvapi32"
set "MSVC_LIBS=wbemuuid.lib ole32.lib oleaut32.lib tbs.lib advapi32.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [machinfo] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o machinfo.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [machinfo] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [machinfo] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:machinfo.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [machinfo] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [machinfo] Compiling with g++...
    g++ -std=c++17 -O2 -municode %SOURCES% %LIBS% -o machinfo.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [machinfo] Build failed with g++
    exit /b 1
)

echo [machinfo] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [machinfo] Build successful: machinfo.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y machinfo.exe "..\..\bin\machinfo.exe" >nul
    echo [machinfo] Installed to bin\machinfo.exe
)
:done
exit /b 0

:clean
echo [machinfo] Cleaning build artifacts...
del /q machinfo.exe *.obj *.o *.pdb *.ilk 2>nul
echo [machinfo] Clean complete.
exit /b 0
