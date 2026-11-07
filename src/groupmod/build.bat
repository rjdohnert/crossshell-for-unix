@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean
if not "%~1"=="" if /I not "%~1"=="build" if /I not "%~1"=="noinstall" (
    echo [groupmod] Error: unknown action "%~1".
    popd
    exit /b 1
)

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [groupmod] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [groupmod] Compiling with clang++...
clang++ -std=c++17 -O2 groupmod.cpp groupmod_cli.cpp groupmod_group.cpp groupmod_input.cpp groupmod_app.cpp -lnetapi32 -Wl,/subsystem:console -o groupmod.exe
if errorlevel 1 (
    echo [groupmod] Build failed.
    popd
    exit /b 1
)

echo [groupmod] Build successful: groupmod.exe

if /I "%~1"=="noinstall" goto :skip_install
if not exist "..\..\bin" mkdir "..\..\bin"
if errorlevel 1 (
    echo [groupmod] Error: could not create the bin directory.
    popd
    exit /b 1
)
copy /y groupmod.exe "..\..\bin\groupmod.exe" >nul
if errorlevel 1 (
    echo [groupmod] Error: could not install groupmod.exe to bin.
    popd
    exit /b 1
)
echo [groupmod] Installed to ..\..\bin\groupmod.exe

:skip_install
popd
exit /b 0

:clean
del /q groupmod.exe *.obj *.o *.pdb *.ilk 2>nul
echo [groupmod] Clean complete.
popd
exit /b 0
