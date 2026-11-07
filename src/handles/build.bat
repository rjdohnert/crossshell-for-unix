@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [handles] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [handles] Compiling with clang++...
clang++ -std=c++17 -O2 handles.cpp handles_cli.cpp handles_collect.cpp handles_format.cpp handles_app.cpp -Wl,/subsystem:console -o handles.exe
if errorlevel 1 (
    echo [handles] Build failed.
    popd
    exit /b 1
)

echo [handles] Build successful: handles.exe

if /I "%~1"=="noinstall" goto :skip_install
if not exist "..\..\bin" mkdir "..\..\bin"
if errorlevel 1 (
    echo [handles] Error: could not create the bin directory.
    popd
    exit /b 1
)
copy /y handles.exe "..\..\bin\handles.exe" >nul
if errorlevel 1 (
    echo [handles] Error: could not install handles.exe to bin.
    popd
    exit /b 1
)
echo [handles] Installed to ..\..\bin\handles.exe

:skip_install
popd
exit /b 0

:clean
del /q handles.exe *.obj *.o *.pdb *.ilk 2>nul
echo [handles] Clean complete.
popd
exit /b 0
