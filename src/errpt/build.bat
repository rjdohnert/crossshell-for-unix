@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [errpt] Error: clang++ not found.& popd& exit /b 1)
echo [errpt] Compiling with clang++...
clang++ -std=c++17 -O2 errpt.cpp -Wl,/subsystem:console -o errpt.exe
if errorlevel 1 (echo [errpt] Build failed.& popd& exit /b 1)
echo [errpt] Build successful: errpt.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y errpt.exe "..\..\bin\errpt.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q errpt.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
