@echo off
setlocal

set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [cat] Error: clang++ was not found in PATH.
    popd
    exit /b 1
)

echo [cat] Compiling with clang++...
clang++ -std=c++17 -O2 cat.cpp cat_options.cpp cat_formatter.cpp cat_help.cpp cat_arguments.cpp cat_engine.cpp -Wl,/subsystem:console -o cat.exe
if errorlevel 1 (
    echo [cat] Build failed.
    popd
    exit /b 1
)

echo [cat] Build successful: cat.exe
if exist "..\..\bin" (
    copy /y cat.exe "..\..\bin\cat.exe" >nul
    echo [cat] Installed to bin\cat.exe
)
popd
exit /b 0

:clean
echo [cat] Cleaning build artifacts...
del /q cat.exe *.obj *.o *.pdb *.ilk 2>nul
echo [cat] Clean complete.
popd
exit /b 0