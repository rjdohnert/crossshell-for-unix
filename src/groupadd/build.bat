@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean
if not "%~1"=="" if /I not "%~1"=="build" if /I not "%~1"=="noinstall" (
    echo [groupadd] Error: unknown action "%~1".
    popd
    exit /b 1
)

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [groupadd] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [groupadd] Compiling with clang++...
clang++ -std=c++17 -O2 groupadd.cpp groupadd_cli.cpp groupadd_group.cpp groupadd_input.cpp groupadd_app.cpp -lnetapi32 -Wl,/subsystem:console -o groupadd.exe
if errorlevel 1 (
    echo [groupadd] Build failed.
    popd
    exit /b 1
)

echo [groupadd] Build successful: groupadd.exe

if /I "%~1"=="noinstall" goto :skip_install
if not exist "..\..\bin" mkdir "..\..\bin"
if errorlevel 1 (
    echo [groupadd] Error: could not create the bin directory.
    popd
    exit /b 1
)
copy /y groupadd.exe "..\..\bin\groupadd.exe" >nul
if errorlevel 1 (
    echo [groupadd] Error: could not install groupadd.exe to bin.
    popd
    exit /b 1
)
echo [groupadd] Installed to ..\..\bin\groupadd.exe

:skip_install
popd
exit /b 0

:clean
del /q groupadd.exe *.obj *.o *.pdb *.ilk 2>nul
echo [groupadd] Clean complete.
popd
exit /b 0
