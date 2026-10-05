@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [cmake] Error: clang++ not found.& popd& exit /b 1)
echo [cmake] Compiling with clang++...
clang++ -std=c++17 -O2 cmake.cpp -Wl,/subsystem:console -o cmake.exe
if errorlevel 1 (echo [cmake] Build failed.& popd& exit /b 1)
echo [cmake] Build successful: cmake.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y cmake.exe "..\..\bin\cmake.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q cmake.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
