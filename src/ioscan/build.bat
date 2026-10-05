@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [ioscan] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [ioscan] Compiling with clang++...
clang++ -std=c++17 -O2 ioscan.cpp -lsetupapi -lcfgmgr32 -Wl,/subsystem:console -o ioscan.exe
if errorlevel 1 (
    echo [ioscan] Build failed.
    popd
    exit /b 1
)

echo [ioscan] Build successful: ioscan.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y ioscan.exe "..\..\bin\ioscan.exe" >nul
    echo [ioscan] Installed to ..\..\bin\ioscan.exe
)

:skip_install
popd
exit /b 0

:clean
del /q ioscan.exe *.obj *.o *.pdb *.ilk 2>nul
echo [ioscan] Clean complete.
popd
exit /b 0
