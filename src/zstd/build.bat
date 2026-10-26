@echo off
setlocal

echo ========================================================
echo Building zstd - CrossShell RFC 8878 Enterprise
echo ========================================================

pushd "%~dp0"
if not exist "..\..\bin" mkdir "..\..\bin"

set "SOURCES=zstd.c xxh64.c codec.c pool.c engine.c zstddeclib.c"
set "TARGET=..\..\bin\zstd.exe"

where clang >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo Compiling with Clang -O3...
    clang -O3 -Wall -Wextra -Wno-unused-function -Wno-unused-const-variable %SOURCES% -o "%TARGET%"
    goto check_result
)

where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo Compiling with MSVC cl /O2...
    cl /O2 /W3 /D_CRT_SECURE_NO_WARNINGS %SOURCES% /Fe:"%TARGET%" /link
    goto check_result
)

where gcc >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo Compiling with GCC -O3...
    gcc -O3 -Wall -Wextra %SOURCES% -o "%TARGET%"
    goto check_result
)

echo ERROR: No supported C compiler found (clang, cl, gcc).
popd
exit /b 1

:check_result
set "ERR=%ERRORLEVEL%"
popd
if %ERR% equ 0 (
    echo [SUCCESS] Built zstd.exe in bin\
    exit /b 0
) else (
    echo [FAILURE] Compilation failed with error code %ERR%
    exit /b %ERR%
)
