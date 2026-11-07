@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [gzip] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [gzip] Compiling with clang++...
clang++ -std=c++17 -O2 gzip.cpp gzip_cli.cpp gzip_codec.cpp gzip_file.cpp gzip_member.cpp gzip_report.cpp gzip_engine.cpp gzip_app.cpp -Wl,/subsystem:console -o gzip.exe
if errorlevel 1 (
    echo [gzip] Build failed.
    popd
    exit /b 1
)

echo [gzip] Build successful: gzip.exe

if not exist "..\..\bin" mkdir "..\..\bin"
if errorlevel 1 (
    echo [gzip] Error: could not create the bin directory.
    popd
    exit /b 1
)
copy /y gzip.exe "..\..\bin\gzip.exe" >nul
if errorlevel 1 (
    echo [gzip] Error: could not install gzip.exe to bin.
    popd
    exit /b 1
)
echo [gzip] Installed to ..\..\bin\gzip.exe

popd
exit /b 0

:clean
del /q gzip.exe *.obj *.o *.pdb *.ilk 2>nul
echo [gzip] Clean complete.
popd
exit /b 0
