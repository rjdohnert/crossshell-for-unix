@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [csplit] Error: clang++ not found.& popd& exit /b 1)
echo [csplit] Compiling with clang++...
clang++ -std=c++17 -O2 csplit.cpp -Wl,/subsystem:console -o csplit.exe
if errorlevel 1 (echo [csplit] Build failed.& popd& exit /b 1)
echo [csplit] Build successful: csplit.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y csplit.exe "..\..\bin\csplit.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q csplit.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
