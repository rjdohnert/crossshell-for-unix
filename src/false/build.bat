@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [false] Error: clang++ not found.& popd& exit /b 1)
echo [false] Compiling with clang++...
clang++ -std=c++17 -O2 false.cpp -Wl,/subsystem:console -o false.exe
if errorlevel 1 (echo [false] Build failed.& popd& exit /b 1)
echo [false] Build successful: false.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y false.exe "..\..\bin\false.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q false.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
