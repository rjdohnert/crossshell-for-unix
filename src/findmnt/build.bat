@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [findmnt] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [findmnt] Compiling with clang++...
clang++ -std=c++17 -O2 findmnt.cpp -lmpr -Wl,/subsystem:console -o findmnt.exe
if errorlevel 1 (
    echo [findmnt] Build failed.
    popd
    exit /b 1
)

echo [findmnt] Build successful: findmnt.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y findmnt.exe "..\..\bin\findmnt.exe" >nul
    echo [findmnt] Installed to ..\..\bin\findmnt.exe
)

:skip_install
popd
exit /b 0

:clean
del /q findmnt.exe *.obj *.o *.pdb *.ilk 2>nul
echo [findmnt] Clean complete.
popd
exit /b 0
