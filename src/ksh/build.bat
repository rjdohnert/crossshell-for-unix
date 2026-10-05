@echo off
setlocal enabledelayedexpansion
set "SRC_DIR=%~dp0"
pushd "%SRC_DIR%" || exit /b 1

if /I "%~1"=="clean" goto :clean
if /I "%~1"=="amalgamate" goto :amalgamate
if /I "%~1"=="modular" goto :build_modular

:: Detect C++ compiler
set "COMPILER="
where clang++ >nul 2>nul
if not errorlevel 1 (
    set "COMPILER=clang++"
) else (
    if exist "C:\Program Files\LLVM\bin\clang++.exe" (
        set "COMPILER="C:\Program Files\LLVM\bin\clang++.exe""
    ) else (
        where cl >nul 2>nul
        if not errorlevel 1 (
            set "COMPILER=cl"
        ) else (
            echo [ksh] Error: Neither clang++ nor cl.exe found in PATH.
            popd
            exit /b 1
        )
    )
)

:: Ensure ksh.cpp exists
if not exist "ksh.cpp" (
    echo [ksh] Generating amalgamation...
    call :run_amalgamate
)

echo [ksh] Compiling amalgamated ksh.exe with !COMPILER!...
if "!COMPILER!"=="cl" (
    cl /std:c++17 /O2 /EHsc /nologo /Fe:ksh.exe ksh.cpp Advapi32.lib User32.lib
) else (
    !COMPILER! -std=c++17 -O2 ksh.cpp -lAdvapi32 -lUser32 -Wl,/subsystem:console -o ksh.exe
)
if errorlevel 1 (
    echo [ksh] Build failed.
    popd
    exit /b 1
)

echo [ksh] Build successful: ksh.exe

if /I "%~1"=="test" (
    echo [ksh] Running self-tests...
    ksh.exe --self-test
    if errorlevel 1 (
        echo [ksh] Self-tests failed.
        popd
        exit /b 1
    )
)

if /I "%~1"=="noinstall" goto :skip_install
if exist "..\..\bin" (
    copy /y ksh.exe "..\..\bin\ksh.exe" >nul
    echo [ksh] Installed to ..\..\bin\ksh.exe
)

:skip_install
popd
exit /b 0

:build_modular
echo [ksh] Compiling modular ksh_modular.exe...
where clang++ >nul 2>nul
if not errorlevel 1 (
    clang++ -std=c++17 -O2 -I include src\*.cpp -lAdvapi32 -lUser32 -Wl,/subsystem:console -o ksh_modular.exe
) else (
    if exist "C:\Program Files\LLVM\bin\clang++.exe" (
        "C:\Program Files\LLVM\bin\clang++.exe" -std=c++17 -O2 -I include src\*.cpp -lAdvapi32 -lUser32 -Wl,/subsystem:console -o ksh_modular.exe
    ) else (
        cl /std:c++17 /O2 /EHsc /nologo /I include /Fe:ksh_modular.exe src\*.cpp Advapi32.lib User32.lib
    )
)
if errorlevel 1 (
    echo [ksh] Modular build failed.
    popd
    exit /b 1
)
echo [ksh] Modular build successful: ksh_modular.exe

if /I "%~2"=="test" (
    echo [ksh] Running modular self-tests...
    ksh_modular.exe --self-test
    if errorlevel 1 (
        echo [ksh] Modular self-tests failed.
        popd
        exit /b 1
    )
)
popd
exit /b 0

:amalgamate
call :run_amalgamate
popd
exit /b 0

:run_amalgamate
if exist "amalgamate.exe" (
    amalgamate.exe --source-dir . --output ksh.cpp
) else (
    where pwsh >nul 2>nul
    if not errorlevel 1 (
        pwsh -NoProfile -File amalgamate.ps1 -OutputFile ksh.cpp -SourceDir .
    ) else (
        powershell -NoProfile -File amalgamate.ps1 -OutputFile ksh.cpp -SourceDir .
    )
)
exit /b 0

:clean
del /q ksh.exe ksh_modular.exe amalgamate.exe *.obj *.o *.pdb *.ilk 2>nul
echo [ksh] Clean complete.
popd
exit /b 0
