@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [groupdel] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [groupdel] Compiling with clang++...
clang++ -std=c++17 -O2 groupdel.cpp -lnetapi32 -Wl,/subsystem:console -o groupdel.exe
if errorlevel 1 (
    echo [groupdel] Build failed.
    popd
    exit /b 1
)

echo [groupdel] Build successful: groupdel.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y groupdel.exe "..\..\bin\groupdel.exe" >nul
    echo [groupdel] Installed to ..\..\bin\groupdel.exe
)

:skip_install
popd
exit /b 0

:clean
del /q groupdel.exe *.obj *.o *.pdb *.ilk 2>nul
echo [groupdel] Clean complete.
popd
exit /b 0
