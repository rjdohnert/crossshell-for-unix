@echo off
setlocal

set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [cbasic] Error: clang++ was not found in PATH.
    popd
    exit /b 1
)

echo [cbasic] Compiling with clang++...
clang++ -std=c++17 -O2 cbasic.cpp cbasic_helpers.cpp cbasic_lexer.cpp cbasic_cli.cpp "-Wl,/subsystem:console" -o cbasic.exe
if errorlevel 1 (
    echo [cbasic] Build failed.
    popd
    exit /b 1
)

echo [cbasic] Build successful: cbasic.exe
if exist "..\..\bin" (
    copy /y cbasic.exe "..\..\bin\cbasic.exe" >nul
    echo [cbasic] Installed to bin\cbasic.exe
)
popd
exit /b 0

:clean
echo [cbasic] Cleaning build artifacts...
del /q cbasic.exe *.obj *.o *.pdb *.ilk 2>nul
echo [cbasic] Clean complete.
popd
exit /b 0
