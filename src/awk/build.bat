@echo off
setlocal enabledelayedexpansion

echo [awk] Building awk...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=awk.cpp awk_app.cpp context.cpp evaluator.cpp parser.cpp string_utils.cpp value.cpp win_loader.cpp"
set "LIBS=-lwbemuuid -lole32 -loleaut32 -ladvapi32"
set "MSVC_LIBS=wbemuuid.lib ole32.lib oleaut32.lib advapi32.lib"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [awk] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% %LIBS% -o awk.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [awk] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [awk] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% %MSVC_LIBS% /Fe:awk.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [awk] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [awk] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% %LIBS% -o awk.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [awk] Build failed with g++
    exit /b 1
)

echo [awk] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [awk] Build successful: awk.exe
if exist "..\..\bin" (
    copy /y awk.exe "..\..\bin\awk.exe" >nul
    echo [awk] Installed to bin\awk.exe
)
exit /b 0

:clean
echo [awk] Cleaning build artifacts...
del /q awk.exe *.obj *.o *.pdb *.ilk 2>nul
echo [awk] Clean complete.
exit /b 0
