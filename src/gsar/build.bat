@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [gsar] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [gsar] Compiling with clang++...
clang++ -std=c++17 -O2 gsar.cpp -Wl,/subsystem:console -o gsar.exe
if errorlevel 1 (
    echo [gsar] Build failed.
    popd
    exit /b 1
)

echo [gsar] Build successful: gsar.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y gsar.exe "..\..\bin\gsar.exe" >nul
    echo [gsar] Installed to ..\..\bin\gsar.exe
)

:skip_install
popd
exit /b 0

:clean
del /q gsar.exe *.obj *.o *.pdb *.ilk 2>nul
echo [gsar] Clean complete.
popd
exit /b 0
