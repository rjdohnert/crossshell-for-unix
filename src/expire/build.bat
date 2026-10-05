@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [expire] Error: clang++ not found.& popd& exit /b 1)
echo [expire] Compiling with clang++...
clang++ -std=c++17 -O2 expire.cpp -Wl,/subsystem:console -o expire.exe
if errorlevel 1 (echo [expire] Build failed.& popd& exit /b 1)
echo [expire] Build successful: expire.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y expire.exe "..\..\bin\expire.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q expire.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
