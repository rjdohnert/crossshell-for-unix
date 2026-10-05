@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [hostid] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [hostid] Compiling with clang++...
clang++ -std=c++17 -O2 hostid.cpp -lws2_32 -ladvapi32 -Wl,/subsystem:console -o hostid.exe
if errorlevel 1 (
    echo [hostid] Build failed.
    popd
    exit /b 1
)

echo [hostid] Build successful: hostid.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y hostid.exe "..\..\bin\hostid.exe" >nul
    echo [hostid] Installed to ..\..\bin\hostid.exe
)

:skip_install
popd
exit /b 0

:clean
del /q hostid.exe *.obj *.o *.pdb *.ilk 2>nul
echo [hostid] Clean complete.
popd
exit /b 0
