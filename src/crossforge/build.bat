@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [crossforge] Error: clang++ not found.& popd& exit /b 1)
echo [crossforge] Compiling with clang++...
clang++ -std=c++17 -O2 crossforge.cpp -Wl,/subsystem:console -luser32 -o crossforge.exe
if errorlevel 1 (echo [crossforge] Build failed.& popd& exit /b 1)
echo [crossforge] Build successful: crossforge.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y crossforge.exe "..\..\bin\crossforge.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q crossforge.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
