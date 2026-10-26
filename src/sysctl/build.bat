@echo off
setlocal enabledelayedexpansion

echo [sysctl] Building sysctl...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=sysctl.cpp sysctl_app.cpp telemetry_provider.cpp storage_provider.cpp reporter.cpp options.cpp mib_tree.cpp common.cpp"
set "LIBS=-liphlpapi -ladvapi32 -lsetupapi -luuid -lpsapi"
set "MSVC_LIBS=iphlpapi.lib advapi32.lib setupapi.lib uuid.lib psapi.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [sysctl] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -Wl,/subsystem:console -o sysctl.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [sysctl] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [sysctl] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:sysctl.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [sysctl] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [sysctl] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o sysctl.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [sysctl] Build failed with g++
    exit /b 1
)

echo [sysctl] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [sysctl] Build successful: sysctl.exe
if "%1"=="noinstall" goto :done
if exist "..\..\bin" (
    copy /y sysctl.exe "..\..\bin\sysctl.exe" >nul
    echo [sysctl] Installed to bin\sysctl.exe
)
:done
exit /b 0

:clean
echo [sysctl] Cleaning build artifacts...
del /q sysctl.exe *.obj *.o *.pdb *.ilk sysctl-tests.exe 2>nul
echo [sysctl] Clean complete.
exit /b 0
