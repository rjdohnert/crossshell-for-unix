@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [kgdb] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [kgdb] Compiling with clang++...
clang++ -std=c++17 -O2 kgdb.cpp -lws2_32 -lpsapi -Wl,/subsystem:console -o kgdb.exe
if errorlevel 1 (
    echo [kgdb] Build failed.
    popd
    exit /b 1
)

echo [kgdb] Build successful: kgdb.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y kgdb.exe "..\..\bin\kgdb.exe" >nul
    echo [kgdb] Installed to ..\..\bin\kgdb.exe
)

:skip_install
popd
exit /b 0

:clean
del /q kgdb.exe *.obj *.o *.pdb *.ilk 2>nul
echo [kgdb] Clean complete.
popd
exit /b 0
