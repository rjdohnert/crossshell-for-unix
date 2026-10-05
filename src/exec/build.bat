@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [exec] Error: clang++ not found.& popd& exit /b 1)
echo [exec] Compiling with clang++...
clang++ -std=c++17 -O2 exec.cpp -Wl,/subsystem:console -o exec.exe
if errorlevel 1 (echo [exec] Build failed.& popd& exit /b 1)
echo [exec] Build successful: exec.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y exec.exe "..\..\bin\exec.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q exec.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
