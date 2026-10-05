@echo off
setlocal
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean

where clang++ >nul 2>nul
if errorlevel 1 (
    echo [groupadd] Error: clang++ not found in PATH.
    popd
    exit /b 1
)

echo [groupadd] Compiling with clang++...
clang++ -std=c++17 -O2 groupadd.cpp -lnetapi32 -Wl,/subsystem:console -o groupadd.exe
if errorlevel 1 (
    echo [groupadd] Build failed.
    popd
    exit /b 1
)

echo [groupadd] Build successful: groupadd.exe

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y groupadd.exe "..\..\bin\groupadd.exe" >nul
    echo [groupadd] Installed to ..\..\bin\groupadd.exe
)

:skip_install
popd
exit /b 0

:clean
del /q groupadd.exe *.obj *.o *.pdb *.ilk 2>nul
echo [groupadd] Clean complete.
popd
exit /b 0
