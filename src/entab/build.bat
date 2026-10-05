@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [entab] Error: clang++ not found.& popd& exit /b 1)
echo [entab] Compiling with clang++...
clang++ -std=c++17 -O2 entab.cpp -Wl,/subsystem:console -o entab.exe
if errorlevel 1 (echo [entab] Build failed.& popd& exit /b 1)
echo [entab] Build successful: entab.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y entab.exe "..\..\bin\entab.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q entab.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
