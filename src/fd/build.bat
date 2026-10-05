@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [fd] Error: clang++ not found.& popd& exit /b 1)
echo [fd] Compiling with clang++...
clang++ -std=c++17 -O2 fd.cpp -Wl,/subsystem:console -o fd.exe
if errorlevel 1 (echo [fd] Build failed.& popd& exit /b 1)
echo [fd] Build successful: fd.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y fd.exe "..\..\bin\fd.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q fd.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
