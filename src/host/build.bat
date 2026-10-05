@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [host] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [host] Compiling with clang++...
clang++ -std=c++17 -O2 host.cpp -ldnsapi -lws2_32 -Wl,/subsystem:console -o host.exe
if errorlevel 1 (
    echo [host] Build failed.
    popd
    exit /b 1
)

echo [host] Build successful: host.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y host.exe "..\..\bin\host.exe" >nul
    echo [host] Installed to ..\..\bin\host.exe
)

:skip_install
popd
exit /b 0

:clean
del /q host.exe *.obj *.o *.pdb *.ilk 2>nul
echo [host] Clean complete.
popd
exit /b 0
