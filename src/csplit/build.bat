@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang >nul 2>nul
if errorlevel 1 (echo [csplit] Error: clang not found.& popd& exit /b 1)
echo [csplit] Compiling with clang...
clang -std=c17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX csplit.c csplit_engine.c dynamic_string.c output_file_manager.c pattern.c regex_engine.c -o csplit.exe
if errorlevel 1 (echo [csplit] Build failed.& popd& exit /b 1)
echo [csplit] Build successful: csplit.exe
if /I "%~1"=="noinstall" goto :skip_install
if not exist "..\..\bin" mkdir "..\..\bin"
if errorlevel 1 (echo [csplit] Could not create bin directory.& popd& exit /b 1)
copy /y csplit.exe "..\..\bin\csplit.exe" >nul
if errorlevel 1 (echo [csplit] Could not install executable in bin.& popd& exit /b 1)
:skip_install
popd
exit /b 0
:clean
del /q csplit.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
