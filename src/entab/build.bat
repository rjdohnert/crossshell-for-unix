@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [entab] Error: clang++ not found.& popd& exit /b 1)
echo [entab] Compiling modules...
clang++ -std=c++17 -O2 entab.cpp entab_app.cpp entab_arg_parser.cpp entab_engine.cpp entab_help.cpp entab_reporter.cpp -Wl,/subsystem:console -o entab.exe
if errorlevel 1 (echo [entab] Build failed.& popd& exit /b 1)
echo [entab] Build successful: entab.exe
if /I "%~1"=="noinstall" goto :skip_install
if not exist "..\..\bin" mkdir "..\..\bin"
if errorlevel 1 (echo [entab] Error: could not create bin directory.& popd& exit /b 1)
copy /y entab.exe "..\..\bin\entab.exe" >nul
if errorlevel 1 (echo [entab] Error: could not install entab.exe to bin\.& popd& exit /b 1)
:skip_install
popd
exit /b 0
:clean
del /q entab.exe entab *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
