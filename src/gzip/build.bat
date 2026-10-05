@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [gzip] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [gzip] Compiling with clang++...
clang++ -std=c++17 -O2 gzip.cpp -Wl,/subsystem:console -o gzip.exe
if errorlevel 1 (
    echo [gzip] Build failed.
    popd
    exit /b 1
)

echo [gzip] Build successful: gzip.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y gzip.exe "..\..\bin\gzip.exe" >nul
    echo [gzip] Installed to ..\..\bin\gzip.exe
)

:skip_install
popd
exit /b 0

:clean
del /q gzip.exe *.obj *.o *.pdb *.ilk 2>nul
echo [gzip] Clean complete.
popd
exit /b 0
