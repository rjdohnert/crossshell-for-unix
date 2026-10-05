@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [kill] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [kill] Compiling with clang++...
clang++ -std=c++17 -O2 kill.cpp -ladvapi32 -luser32 -Wl,/subsystem:console -o kill.exe
if errorlevel 1 (
    echo [kill] Build failed.
    popd
    exit /b 1
)

echo [kill] Build successful: kill.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y kill.exe "..\..\bin\kill.exe" >nul
    echo [kill] Installed to ..\..\bin\kill.exe
)

:skip_install
popd
exit /b 0

:clean
del /q kill.exe *.obj *.o *.pdb *.ilk 2>nul
echo [kill] Clean complete.
popd
exit /b 0
