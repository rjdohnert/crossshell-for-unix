@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang++ >nul 2>nul
if errorlevel 1 (echo [du] Error: clang++ not found.& popd& exit /b 1)
echo [du] Compiling modules...
clang++ -std=c++17 -O2 du.cpp du_app.cpp du_arg_parser.cpp du_help_system.cpp du_input_stream_reader.cpp du_path_filter.cpp du_reporter.cpp du_scanner.cpp du_size_formatter.cpp -Wl,/subsystem:console -o du.exe
if errorlevel 1 (echo [du] Build failed.& popd& exit /b 1)
echo [du] Build successful: du.exe
if /I "%~1"=="noinstall" goto :skip_install
if not exist "..\..\bin" mkdir "..\..\bin"
if errorlevel 1 (echo [du] Error: could not create bin directory.& popd& exit /b 1)
copy /y du.exe "..\..\bin\du.exe" >nul
if errorlevel 1 (echo [du] Error: could not install du.exe to bin\.& popd& exit /b 1)
:skip_install
popd
exit /b 0
:clean
del /q du.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
