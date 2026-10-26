@echo off
setlocal EnableExtensions
set "SRC_DIR=%~dp0"
set "SOURCE_FILES=ls.c security.c files.c scan.c listing.c"
set "NO_INSTALL=0"
set "CLEAN=0"

:parse
if "%~1"=="" goto configure
if /i "%~1"=="clean" goto set_clean
if /i "%~1"=="--clean" goto set_clean
if /i "%~1"=="--no-install" goto set_no_install
echo Unknown option: %~1
exit /b 1

:set_clean
set "CLEAN=1"
shift /1
goto parse

:set_no_install
set "NO_INSTALL=1"
shift /1
goto parse

:configure
pushd "%SRC_DIR%" || exit /b 1
if "%CLEAN%"=="1" goto clean
where clang >nul 2>nul
if not errorlevel 1 goto clang_build
where cl.exe >nul 2>nul
if not errorlevel 1 goto msvc_build
echo A Windows C compiler is required: clang or cl.exe.
set "RESULT=1"
goto done

:clang_build
clang -std=c17 -O2 -Wall -Wextra %SOURCE_FILES% -ladvapi32 -o ls.exe
goto compiled

:msvc_build
cl.exe /nologo /std:c17 /TC /O2 /W4 /Fe:ls.exe %SOURCE_FILES% Advapi32.lib

:compiled
set "RESULT=%errorlevel%"
if not "%RESULT%"=="0" goto done
if "%NO_INSTALL%"=="1" goto done
if not exist "..\..\bin" mkdir "..\..\bin"
copy /y ls.exe "..\..\bin\ls.exe" >nul
set "RESULT=%errorlevel%"
goto done

:clean
del /q ls.exe *.obj *.o *.pdb *.ilk 2>nul
set "RESULT=0"

:done
popd
exit /b %RESULT%