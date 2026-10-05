@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [fold] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [fold] Compiling with clang++...
clang++ -std=c++17 -O2 fold.cpp -Wl,/subsystem:console -o fold.exe
if errorlevel 1 (
    echo [fold] Build failed.
    popd
    exit /b 1
)

echo [fold] Build successful: fold.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y fold.exe "..\..\bin\fold.exe" >nul
    echo [fold] Installed to ..\..\bin\fold.exe
)

:skip_install
popd
exit /b 0

:clean
del /q fold.exe *.obj *.o *.pdb *.ilk 2>nul
echo [fold] Clean complete.
popd
exit /b 0
