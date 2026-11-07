@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean
if not "%~1"=="" if /I not "%~1"=="build" if /I not "%~1"=="noinstall" (
    echo [groupdel] Error: unknown action "%~1".
    popd
    exit /b 1
)

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [groupdel] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [groupdel] Compiling with clang++...
clang++ -std=c++17 -O2 groupdel.cpp groupdel_cli.cpp groupdel_group.cpp groupdel_input.cpp groupdel_app.cpp -lnetapi32 -Wl,/subsystem:console -o groupdel.exe
if errorlevel 1 (
    echo [groupdel] Build failed.
    popd
    exit /b 1
)

echo [groupdel] Build successful: groupdel.exe

if /I "%~1"=="noinstall" goto :skip_install
if not exist "..\..\bin" mkdir "..\..\bin"
if errorlevel 1 (
    echo [groupdel] Error: could not create the bin directory.
    popd
    exit /b 1
)
copy /y groupdel.exe "..\..\bin\groupdel.exe" >nul
if errorlevel 1 (
    echo [groupdel] Error: could not install groupdel.exe to bin.
    popd
    exit /b 1
)
echo [groupdel] Installed to ..\..\bin\groupdel.exe

:skip_install
popd
exit /b 0

:clean
del /q groupdel.exe *.obj *.o *.pdb *.ilk 2>nul
echo [groupdel] Clean complete.
popd
exit /b 0
