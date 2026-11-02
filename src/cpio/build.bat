@echo off
setlocal EnableExtensions
set TOOL_NAME=cpio
set SOURCE_FILES=cpio.c cpio_path.c cpio_inode.c cpio_format.c cpio_stream.c cpio_security.c cpio_engine.c cpio_cli.c
set NO_INSTALL=0
set CLEAN=0
:parse
if "%~1"=="" goto configure
if /i "%~1"=="clean" goto set_clean
if /i "%~1"=="--clean" goto set_clean
if /i "%~1"=="-Clean" goto set_clean
if /i "%~1"=="--no-install" goto set_no_install
if /i "%~1"=="-NoInstall" goto set_no_install
if /i "%~1"=="noinstall" goto set_no_install
echo Unknown option: %1
exit /b 1
:set_clean
set CLEAN=1
shift /1
goto parse
:set_no_install
set NO_INSTALL=1
shift /1
goto parse
:configure
pushd "%~dp0" || exit /b 1
if "%CLEAN%"=="1" goto clean
where clang >nul 2>nul
if not errorlevel 1 goto clang
where cl.exe >nul 2>nul
if not errorlevel 1 goto msvc
where gcc >nul 2>nul
if not errorlevel 1 goto gcc
echo No suitable C compiler found (clang, cl.exe, or gcc required).
set RESULT=1
goto done
:clang
clang -std=c99 -O2 -D_CRT_SECURE_NO_WARNINGS -o "%TOOL_NAME%.exe" %SOURCE_FILES% -lshell32
goto compiled
:msvc
cl.exe /nologo /std:c17 /O2 /D_CRT_SECURE_NO_WARNINGS "/Fe:%TOOL_NAME%.exe" %SOURCE_FILES% shell32.lib
goto compiled
:gcc
gcc -std=c99 -O2 -D_CRT_SECURE_NO_WARNINGS -o "%TOOL_NAME%.exe" %SOURCE_FILES% -lshell32
:compiled
set RESULT=%errorlevel%
if not "%RESULT%"=="0" goto done
echo [%TOOL_NAME%] Build successful: %TOOL_NAME%.exe
if "%NO_INSTALL%"=="1" goto done
if not exist "..\..\bin" mkdir "..\..\bin"
copy /y "%TOOL_NAME%.exe" "..\..\bin\%TOOL_NAME%.exe" >nul
set RESULT=%errorlevel%
if "%RESULT%"=="0" echo [%TOOL_NAME%] Installed to ..\..\bin\%TOOL_NAME%.exe
goto done
:clean
del /q *.obj *.o "%TOOL_NAME%.exe" *.pdb *.ilk 2>nul
set RESULT=0
:done
popd
exit /b %RESULT%
