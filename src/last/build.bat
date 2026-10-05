@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [last] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [last] Compiling with clang++...
clang++ -std=c++17 -O2 last.cpp -lwevtapi -ladvapi32 -Wl,/subsystem:console -o last.exe
if errorlevel 1 (
    echo [last] Build failed.
    popd
    exit /b 1
)

echo [last] Build successful: last.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y last.exe "..\..\bin\last.exe" >nul
    echo [last] Installed to ..\..\bin\last.exe
)

:skip_install
popd
exit /b 0

:clean
del /q last.exe *.obj *.o *.pdb *.ilk 2>nul
echo [last] Clean complete.
popd
exit /b 0
