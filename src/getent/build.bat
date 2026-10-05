@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [getent] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [getent] Compiling with clang++...
clang++ -std=c++17 -O2 getent.cpp -lnetapi32 -lws2_32 -liphlpapi -ladvapi32 -Wl,/subsystem:console -o getent.exe
if errorlevel 1 (
    echo [getent] Build failed.
    popd
    exit /b 1
)

echo [getent] Build successful: getent.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y getent.exe "..\..\bin\getent.exe" >nul
    echo [getent] Installed to ..\..\bin\getent.exe
)

:skip_install
popd
exit /b 0

:clean
del /q getent.exe *.obj *.o *.pdb *.ilk 2>nul
echo [getent] Clean complete.
popd
exit /b 0
