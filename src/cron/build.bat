@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [cron] Error: clang++ not found.& popd& exit /b 1)
echo [cron] Compiling with clang++...
clang++ -std=c++17 -O2 cron.cpp -Wl,/subsystem:console -o cron.exe
if errorlevel 1 (echo [cron] Build failed.& popd& exit /b 1)
echo [cron] Build successful: cron.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y cron.exe "..\..\bin\cron.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q cron.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
