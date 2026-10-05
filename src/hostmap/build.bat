@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [hostmap] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [hostmap] Compiling with clang++...
clang++ -std=c++17 -O2 hostmap.cpp -lws2_32 -liphlpapi -ladvapi32 -Wl,/subsystem:console -o hostmap.exe
if errorlevel 1 (
    echo [hostmap] Build failed.
    popd
    exit /b 1
)

echo [hostmap] Build successful: hostmap.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y hostmap.exe "..\..\bin\hostmap.exe" >nul
    echo [hostmap] Installed to ..\..\bin\hostmap.exe
)

:skip_install
popd
exit /b 0

:clean
del /q hostmap.exe *.obj *.o *.pdb *.ilk 2>nul
echo [hostmap] Clean complete.
popd
exit /b 0
