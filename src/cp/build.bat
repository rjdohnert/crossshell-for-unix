@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [cp] Error: clang++ not found.& popd& exit /b 1)
echo [cp] Compiling with clang++...
clang++ -std=c++17 -O2 cp.cpp -Wl,/subsystem:console -o cp.exe
if errorlevel 1 (echo [cp] Build failed.& popd& exit /b 1)
echo [cp] Build successful: cp.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y cp.exe "..\..\bin\cp.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q cp.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
