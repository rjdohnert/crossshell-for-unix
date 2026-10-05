@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [less] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [less] Compiling with clang++...
clang++ -std=c++17 -O2 less.cpp -Wl,/subsystem:console -o less.exe
if errorlevel 1 (
    echo [less] Build failed.
    popd
    exit /b 1
)

echo [less] Build successful: less.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y less.exe "..\..\bin\less.exe" >nul
    echo [less] Installed to ..\..\bin\less.exe
)

:skip_install
popd
exit /b 0

:clean
del /q less.exe *.obj *.o *.pdb *.ilk 2>nul
echo [less] Clean complete.
popd
exit /b 0
