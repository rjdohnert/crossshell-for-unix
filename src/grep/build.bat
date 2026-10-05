@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [grep] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [grep] Compiling with clang++...
clang++ -std=c++17 -O2 grep.cpp -lwbemuuid -lole32 -loleaut32 -ladvapi32 -Wl,/subsystem:console -o grep.exe
if errorlevel 1 (
    echo [grep] Build failed.
    popd
    exit /b 1
)

echo [grep] Build successful: grep.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y grep.exe "..\..\bin\grep.exe" >nul
    echo [grep] Installed to ..\..\bin\grep.exe
)

:skip_install
popd
exit /b 0

:clean
del /q grep.exe *.obj *.o *.pdb *.ilk 2>nul
echo [grep] Clean complete.
popd
exit /b 0
