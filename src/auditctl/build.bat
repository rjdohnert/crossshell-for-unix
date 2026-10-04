@echo off
setlocal enabledelayedexpansion

echo [auditctl] Building auditctl...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=auditctl.cpp auditctl_app.cpp audit_policy.cpp audit_session.cpp audit_types.cpp cli.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [auditctl] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -ladvapi32 -o auditctl.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [auditctl] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [auditctl] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% Advapi32.lib /Fe:auditctl.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [auditctl] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [auditctl] Compiling with g++...
    g++ -std=c++17 -O2 -municode %SOURCES% -ladvapi32 -o auditctl.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [auditctl] Build failed with g++
    exit /b 1
)

echo [auditctl] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [auditctl] Build successful: auditctl.exe
if exist "..\..\bin" (
    copy /y auditctl.exe "..\..\bin\auditctl.exe" >nul
    echo [auditctl] Installed to bin\auditctl.exe
)
exit /b 0

:clean
echo [auditctl] Cleaning build artifacts...
del /q auditctl.exe *.obj *.o *.pdb *.ilk 2>nul
echo [auditctl] Clean complete.
exit /b 0
