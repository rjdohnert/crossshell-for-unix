@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1
if /I "%~1"=="clean" goto :clean
where clang >nul 2>nul
if errorlevel 1 (echo [fdisk] Error: clang C compiler not found.& popd& exit /b 1)
echo [fdisk] Compiling C sources with clang...
clang -std=c17 -O2 -Wall -Wextra -Wpedantic fdisk.c fdisk_app.c fdisk_device.c fdisk_editor.c fdisk_error.c fdisk_format.c fdisk_image.c fdisk_options.c fdisk_volume.c -Wl,/subsystem:console -lsetupapi -lole32 -lvirtdisk -o fdisk.exe
if errorlevel 1 (echo [fdisk] Build failed.& popd& exit /b 1)
echo [fdisk] Build successful: fdisk.exe
if /I "%~1"=="noinstall" goto :skip_install
if not exist "..\..\bin" mkdir "..\..\bin"
if errorlevel 1 (echo [fdisk] Could not create bin directory.& popd& exit /b 1)
copy /y fdisk.exe "..\..\bin\fdisk.exe" >nul
if errorlevel 1 (echo [fdisk] Could not install fdisk.exe to bin.& popd& exit /b 1)
:skip_install
popd
exit /b 0
:clean
del /q fdisk.exe *.obj *.o *.pdb *.ilk 2>nul
popd
exit /b 0
