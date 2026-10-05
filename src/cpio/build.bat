@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [cpio] Error: clang++ not found.& popd& exit /b 1)
echo [cpio] Compiling with clang++...
clang++ -std=c++17 -O2 cpio.cpp -Wl,/subsystem:console -o cpio.exe
if errorlevel 1 (echo [cpio] Build failed.& popd& exit /b 1)
echo [cpio] Build successful: cpio.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y cpio.exe "..\..\bin\cpio.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q cpio.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
