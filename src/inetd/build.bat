@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [inetd] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [inetd] Compiling with clang++...
clang++ -std=c++17 -O2 inetd.cpp -lws2_32 -lshell32 -ladvapi32 -Wl,/subsystem:console -o inetd.exe
if errorlevel 1 (
    echo [inetd] Build failed.
    popd
    exit /b 1
)

echo [inetd] Build successful: inetd.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y inetd.exe "..\..\bin\inetd.exe" >nul
    echo [inetd] Installed to ..\..\bin\inetd.exe
)

:skip_install
popd
exit /b 0

:clean
del /q inetd.exe *.obj *.o *.pdb *.ilk 2>nul
echo [inetd] Clean complete.
popd
exit /b 0
