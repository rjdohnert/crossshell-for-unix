@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [fdisk] Error: clang++ not found.& popd& exit /b 1)
echo [fdisk] Compiling with clang++...
clang++ -std=c++17 -O2 fdisk.cpp -Wl,/subsystem:console -o fdisk.exe
if errorlevel 1 (echo [fdisk] Build failed.& popd& exit /b 1)
echo [fdisk] Build successful: fdisk.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y fdisk.exe "..\..\bin\fdisk.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q fdisk.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
