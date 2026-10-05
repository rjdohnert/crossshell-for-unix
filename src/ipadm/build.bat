@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [ipadm] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [ipadm] Compiling with clang++...
clang++ -std=c++17 -O2 ipadm.cpp -liphlpapi -lws2_32 -Wl,/subsystem:console -o ipadm.exe
if errorlevel 1 (
    echo [ipadm] Build failed.
    popd
    exit /b 1
)

echo [ipadm] Build successful: ipadm.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y ipadm.exe "..\..\bin\ipadm.exe" >nul
    echo [ipadm] Installed to ..\..\bin\ipadm.exe
)

:skip_install
popd
exit /b 0

:clean
del /q ipadm.exe *.obj *.o *.pdb *.ilk 2>nul
echo [ipadm] Clean complete.
popd
exit /b 0
