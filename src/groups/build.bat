@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [groups] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [groups] Compiling with clang++...
clang++ -std=c++17 -O2 groups.cpp -lnetapi32 -ladvapi32 -Wl,/subsystem:console -o groups.exe
if errorlevel 1 (
    echo [groups] Build failed.
    popd
    exit /b 1
)

echo [groups] Build successful: groups.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y groups.exe "..\..\bin\groups.exe" >nul
    echo [groups] Installed to ..\..\bin\groups.exe
)

:skip_install
popd
exit /b 0

:clean
del /q groups.exe *.obj *.o *.pdb *.ilk 2>nul
echo [groups] Clean complete.
popd
exit /b 0
