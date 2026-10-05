@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [killall] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [killall] Compiling with clang++...
clang++ -std=c++17 -O2 killall.cpp -ladvapi32 -luser32 -Wl,/subsystem:console -o killall.exe
if errorlevel 1 (
    echo [killall] Build failed.
    popd
    exit /b 1
)

echo [killall] Build successful: killall.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y killall.exe "..\..\bin\killall.exe" >nul
    echo [killall] Installed to ..\..\bin\killall.exe
)

:skip_install
popd
exit /b 0

:clean
del /q killall.exe *.obj *.o *.pdb *.ilk 2>nul
echo [killall] Clean complete.
popd
exit /b 0
