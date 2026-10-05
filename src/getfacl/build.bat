@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [getfacl] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [getfacl] Compiling with clang++...
clang++ -std=c++17 -O2 getfacl.cpp -ladvapi32 -Wl,/subsystem:console -o getfacl.exe
if errorlevel 1 (
    echo [getfacl] Build failed.
    popd
    exit /b 1
)

echo [getfacl] Build successful: getfacl.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y getfacl.exe "..\..\bin\getfacl.exe" >nul
    echo [getfacl] Installed to ..\..\bin\getfacl.exe
)

:skip_install
popd
exit /b 0

:clean
del /q getfacl.exe *.obj *.o *.pdb *.ilk 2>nul
echo [getfacl] Clean complete.
popd
exit /b 0
