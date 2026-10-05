@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [join] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [join] Compiling with clang++...
clang++ -std=c++17 -O2 join.cpp -Wl,/subsystem:console -o join.exe
if errorlevel 1 (
    echo [join] Build failed.
    popd
    exit /b 1
)

echo [join] Build successful: join.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y join.exe "..\..\bin\join.exe" >nul
    echo [join] Installed to ..\..\bin\join.exe
)

:skip_install
popd
exit /b 0

:clean
del /q join.exe *.obj *.o *.pdb *.ilk 2>nul
echo [join] Clean complete.
popd
exit /b 0
