@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [iostat] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [iostat] Compiling with clang++...
clang++ -std=c++17 -O2 iostat.cpp -lpdh -Wl,/subsystem:console -o iostat.exe
if errorlevel 1 (
    echo [iostat] Build failed.
    popd
    exit /b 1
)

echo [iostat] Build successful: iostat.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y iostat.exe "..\..\bin\iostat.exe" >nul
    echo [iostat] Installed to ..\..\bin\iostat.exe
)

:skip_install
popd
exit /b 0

:clean
del /q iostat.exe *.obj *.o *.pdb *.ilk 2>nul
echo [iostat] Clean complete.
popd
exit /b 0
