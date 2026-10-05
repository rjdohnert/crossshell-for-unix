@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [file] Error: clang++ not found.& popd& exit /b 1)
echo [file] Compiling with clang++...
clang++ -std=c++17 -O2 file.cpp -Wl,/subsystem:console -o file.exe
if errorlevel 1 (echo [file] Build failed.& popd& exit /b 1)
echo [file] Build successful: file.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y file.exe "..\..\bin\file.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q file.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
