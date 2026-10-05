@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [conexec] Error: clang++ not found.& popd& exit /b 1)
echo [conexec] Compiling with clang++...
clang++ -std=c++17 -O2 conexec.cpp -Wl,/subsystem:console -ladvapi32 -luser32 -lshell32 -luserenv -o conexec.exe
if errorlevel 1 (echo [conexec] Build failed.& popd& exit /b 1)
echo [conexec] Build successful: conexec.exe
if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" copy /y conexec.exe "..\..\bin\conexec.exe" >nul
:skip_install
popd
exit /b 0
:clean
del /q conexec.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
