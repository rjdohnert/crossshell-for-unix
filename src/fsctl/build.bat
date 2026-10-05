@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [fsctl] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [fsctl] Compiling with clang++...
clang++ -std=c++17 -O2 fsctl.cpp -lmpr -lwininet -lshell32 -ladvapi32 -Wl,/subsystem:console -o fsctl.exe
if errorlevel 1 (
    echo [fsctl] Build failed.
    popd
    exit /b 1
)

echo [fsctl] Build successful: fsctl.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y fsctl.exe "..\..\bin\fsctl.exe" >nul
    echo [fsctl] Installed to ..\..\bin\fsctl.exe
)

:skip_install
popd
exit /b 0

:clean
del /q fsctl.exe *.obj *.o *.pdb *.ilk 2>nul
echo [fsctl] Clean complete.
popd
exit /b 0
