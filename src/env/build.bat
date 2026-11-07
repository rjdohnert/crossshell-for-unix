@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [env] Error: clang++ not found.& popd& exit /b 1)
echo [env] Compiling modules with clang++...
clang++ -std=c++17 -O2 env.cpp env_arguments.cpp env_application.cpp env_environment.cpp env_output.cpp env_process.cpp -Wl,/subsystem:console -o env.exe
if errorlevel 1 (echo [env] Build failed.& popd& exit /b 1)
echo [env] Build successful: env.exe
if /I "%~1"=="noinstall" goto :skip_install
if not exist "..\..\bin" mkdir "..\..\bin"
if errorlevel 1 (echo [env] Error: could not create bin directory.& popd& exit /b 1)
copy /y env.exe "..\..\bin\env.exe" >nul
if errorlevel 1 (echo [env] Error: could not install env.exe to bin\.& popd& exit /b 1)
:skip_install
popd
exit /b 0
:clean
del /q env.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
