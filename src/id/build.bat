@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [id] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [id] Compiling with clang++...
clang++ -std=c++17 -O2 id.cpp -ladvapi32 -lnetapi32 -Wl,/subsystem:console -o id.exe
if errorlevel 1 (
    echo [id] Build failed.
    popd
    exit /b 1
)

echo [id] Build successful: id.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y id.exe "..\..\bin\id.exe" >nul
    echo [id] Installed to ..\..\bin\id.exe
)

:skip_install
popd
exit /b 0

:clean
del /q id.exe *.obj *.o *.pdb *.ilk 2>nul
echo [id] Clean complete.
popd
exit /b 0
