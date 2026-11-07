@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [dskctl] Error: clang++ not found.& popd& exit /b 1)
echo [dskctl] Compiling with clang++...
clang++ -std=c++17 -O2 dskctl.cpp dskctl_app.cpp dskctl_engine.cpp dskctl_helpers.cpp dskctl_logger.cpp dskctl_options.cpp dskctl_reporter.cpp dskctl_security.cpp dskctl_targets.cpp -Wl,/subsystem:console -lshell32 -ladvapi32 -lole32 -o dskctl.exe
if errorlevel 1 (echo [dskctl] Build failed.& popd& exit /b 1)
echo [dskctl] Build successful: dskctl.exe
if /I "%~1"=="noinstall" goto :skip_install
if not exist "..\..\bin" mkdir "..\..\bin"
if errorlevel 1 (echo [dskctl] Error: could not create bin directory.& popd& exit /b 1)
copy /y dskctl.exe "..\..\bin\dskctl.exe" >nul
if errorlevel 1 (echo [dskctl] Error: could not install dskctl.exe to bin\.& popd& exit /b 1)
:skip_install
popd
exit /b 0
:clean
del /q dskctl.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
