@echo off
setlocal enabledelayedexpansion

echo [a2pdf] Building a2pdf...

set "SRC_DIR=%~dp0"
cd /d "%SRC_DIR%"

set "SOURCES=a2pdf.cpp a2pdf_app.cpp cli.cpp encoding.cpp output_reporter.cpp pdf_generator.cpp ps_generator.cpp text_processor.cpp"

if "%1"=="clean" goto :clean

:: Check for clang++
where clang++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [a2pdf] Compiling with clang++...
    clang++ -std=c++17 -O2 %SOURCES% -o a2pdf.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [a2pdf] Build failed with clang++
    exit /b 1
)

:: Check for cl.exe
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [a2pdf] Compiling with cl.exe...
    cl /nologo /EHsc /std:c++17 /O2 %SOURCES% /Fe:a2pdf.exe
    if %ERRORLEVEL% equ 0 (
        del *.obj 2>nul
        goto :success
    )
    echo [a2pdf] Build failed with cl.exe
    exit /b 1
)

:: Check for g++
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [a2pdf] Compiling with g++...
    g++ -std=c++17 -O2 %SOURCES% -o a2pdf.exe
    if %ERRORLEVEL% equ 0 goto :success
    echo [a2pdf] Build failed with g++
    exit /b 1
)

echo [a2pdf] Error: No supported C++ compiler found (clang++, cl.exe, g++).
exit /b 1

:success
echo [a2pdf] Build successful: a2pdf.exe
if exist "..\..\bin" (
    copy /y a2pdf.exe "..\..\bin\a2pdf.exe" >nul
    echo [a2pdf] Installed to bin\a2pdf.exe
)
exit /b 0

:clean
echo [a2pdf] Cleaning build artifacts...
del /q a2pdf.exe *.obj *.o *.pdb *.ilk 2>nul
echo [a2pdf] Clean complete.
exit /b 0
