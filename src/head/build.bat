@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [head] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [head] Compiling with clang++...
clang++ -std=c++17 -O2 head.cpp -Wl,/subsystem:console -o head.exe
if errorlevel 1 (
    echo [head] Build failed.
    popd
    exit /b 1
)

echo [head] Build successful: head.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y head.exe "..\..\bin\head.exe" >nul
    echo [head] Installed to ..\..\bin\head.exe
)

:skip_install
popd
exit /b 0

:clean
del /q head.exe *.obj *.o *.pdb *.ilk 2>nul
echo [head] Clean complete.
popd
exit /b 0
