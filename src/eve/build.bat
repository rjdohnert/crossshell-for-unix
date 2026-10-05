@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [eve] Error: clang++ not found.& popd& exit /b 1)
echo [eve] Compiling with clang++...
clang++ -std=c++17 -O2 eve.cpp -Wl,/subsystem:console -o eve.exe
if errorlevel 1 (echo [eve] Build failed.& popd& exit /b 1)
echo [eve] Build successful: eve.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y eve.exe "..\..\bin\eve.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q eve.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
