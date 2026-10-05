@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [driverctl] Error: clang++ not found.& popd& exit /b 1)
echo [driverctl] Compiling with clang++...
clang++ -std=c++17 -O2 driverctl.cpp -Wl,/subsystem:console -o driverctl.exe
if errorlevel 1 (echo [driverctl] Build failed.& popd& exit /b 1)
echo [driverctl] Build successful: driverctl.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y driverctl.exe "..\..\bin\driverctl.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q driverctl.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
