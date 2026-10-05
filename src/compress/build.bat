@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [compress] Error: clang++ not found.& popd& exit /b 1)
echo [compress] Compiling with clang++...
clang++ -std=c++17 -O2 compress.cpp -Wl,/subsystem:console -o compress.exe
if errorlevel 1 (echo [compress] Build failed.& popd& exit /b 1)
echo [compress] Build successful: compress.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y compress.exe "..\..\bin\compress.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q compress.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
