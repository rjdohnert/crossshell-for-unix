@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [du] Error: clang++ not found.& popd& exit /b 1)
echo [du] Compiling with clang++...
clang++ -std=c++17 -O2 du.cpp -Wl,/subsystem:console -o du.exe
if errorlevel 1 (echo [du] Build failed.& popd& exit /b 1)
echo [du] Build successful: du.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y du.exe "..\..\bin\du.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q du.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
