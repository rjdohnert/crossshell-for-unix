@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang >nul 2>nul
if errorlevel 1 (echo [mkfs] Error: clang C compiler not found.& popd& exit /b 1)
echo [mkfs] Compiling C sources with clang...
clang -std=c17 -O2 -Wall -Wextra -Wpedantic mkfs.c mkfs_cli.c mkfs_engine.c mkfs_error.c mkfs_fmifs.c -Wl,/subsystem:console -o mkfs.exe
if errorlevel 1 (echo [mkfs] Build failed.& popd& exit /b 1)
echo [mkfs] Build successful: mkfs.exe
if /I "%~1"=="noinstall" goto :skip_install
if not exist "..\..\bin" mkdir "..\..\bin"
if errorlevel 1 (echo [mkfs] Could not create bin directory.& popd& exit /b 1)
copy /y mkfs.exe "..\..\bin\mkfs.exe" >nul
if errorlevel 1 (echo [mkfs] Could not install mkfs.exe to bin.& popd& exit /b 1)
echo [mkfs] Installed to bin\mkfs.exe
:skip_install
popd
exit /b 0
:clean
del /q mkfs.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
