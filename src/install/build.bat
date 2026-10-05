@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [install] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [install] Compiling with clang++...
clang++ -std=c++17 -O2 install.cpp -Wl,/subsystem:console -o install.exe
if errorlevel 1 (
    echo [install] Build failed.
    popd
    exit /b 1
)

echo [install] Build successful: install.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y install.exe "..\..\bin\install.exe" >nul
    echo [install] Installed to ..\..\bin\install.exe
)

:skip_install
popd
exit /b 0

:clean
del /q install.exe *.obj *.o *.pdb *.ilk 2>nul
echo [install] Clean complete.
popd
exit /b 0
