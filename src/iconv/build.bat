@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [iconv] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [iconv] Compiling with clang++...
clang++ -std=c++17 -O2 iconv.cpp -Wl,/subsystem:console -o iconv.exe
if errorlevel 1 (
    echo [iconv] Build failed.
    popd
    exit /b 1
)

echo [iconv] Build successful: iconv.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y iconv.exe "..\..\bin\iconv.exe" >nul
    echo [iconv] Installed to ..\..\bin\iconv.exe
)

:skip_install
popd
exit /b 0

:clean
del /q iconv.exe *.obj *.o *.pdb *.ilk 2>nul
echo [iconv] Clean complete.
popd
exit /b 0
