@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [cmp] Error: clang++ not found.& popd& exit /b 1)
echo [cmp] Compiling with clang++...
clang++ -std=c++17 -O2 cmp.cpp -Wl,/subsystem:console -o cmp.exe
if errorlevel 1 (echo [cmp] Build failed.& popd& exit /b 1)
echo [cmp] Build successful: cmp.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y cmp.exe "..\..\bin\cmp.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q cmp.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
