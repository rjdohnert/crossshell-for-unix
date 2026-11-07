@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [htop] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [htop] Compiling with clang++...
clang++ -std=c++17 -O2 htop.cpp htop_app.cpp htop_telemetry.cpp htop_console.cpp htop_format.cpp -ladvapi32 -lpsapi -lntdll -Wl,/subsystem:console -o htop.exe
if errorlevel 1 (
    echo [htop] Build failed.
    popd
    exit /b 1
)

echo [htop] Build successful: htop.exe

if /I "%~1"=="noinstall" goto :skip_install
if not exist "..\..\bin" mkdir "..\..\bin"
copy /y htop.exe "..\..\bin\htop.exe" >nul
if errorlevel 1 (
    echo [htop] Failed to install htop.exe to ..\..\bin.
    popd
    exit /b 1
)
echo [htop] Installed to ..\..\bin\htop.exe

:skip_install
popd
exit /b 0

:clean
del /q htop.exe *.obj *.o *.pdb *.ilk 2>nul
echo [htop] Clean complete.
popd
exit /b 0
