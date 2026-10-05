@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [comm] Error: clang++ not found.& popd& exit /b 1)
echo [comm] Compiling with clang++...
clang++ -std=c++17 -O2 comm.cpp -Wl,/subsystem:console -o comm.exe
if errorlevel 1 (echo [comm] Build failed.& popd& exit /b 1)
echo [comm] Build successful: comm.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y comm.exe "..\..\bin\comm.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q comm.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
