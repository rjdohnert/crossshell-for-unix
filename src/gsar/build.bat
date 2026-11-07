@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [gsar] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [gsar] Compiling with clang++...
clang++ -std=c++17 -O2 gsar.cpp gsar_pattern.cpp gsar_cli.cpp gsar_engine.cpp gsar_file.cpp gsar_app.cpp -Wl,/subsystem:console -o gsar.exe
if errorlevel 1 (
    echo [gsar] Build failed.
    popd
    exit /b 1
)

echo [gsar] Build successful: gsar.exe

if not exist "..\..\bin" mkdir "..\..\bin"
if errorlevel 1 (
    echo [gsar] Error: could not create the bin directory.
    popd
    exit /b 1
)
copy /y gsar.exe "..\..\bin\gsar.exe" >nul
if errorlevel 1 (
    echo [gsar] Error: could not install gsar.exe to bin.
    popd
    exit /b 1
)
echo [gsar] Installed to ..\..\bin\gsar.exe

popd
exit /b 0

:clean
del /q gsar.exe *.obj *.o *.pdb *.ilk 2>nul
echo [gsar] Clean complete.
popd
exit /b 0
