@echo off
setlocal EnableExtensions
set SOURCE_FILES=yum.c command_line.c command_line_escaping.c winget_dispatch.c windows_update_query.c privilege_check.c windows_update_install.c package_install.c package_remove.c package_search.c package_info.c package_list.c package_upgrade.c repository_list.c repository_add.c repository_remove.c help.c rpm_dispatch.c package_clean.c
set NO_INSTALL=0
set CLEAN=0
:parse
if "%~1"=="" goto configure
if /i "%~1"=="clean" goto set_clean
if /i "%~1"=="--clean" goto set_clean
if /i "%~1"=="--no-install" goto set_no_install
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
echo A Windows C compiler is required: clang or cl.exe.
set RESULT=1
goto done
:clang
clang -std=c17 -O2 -Wall -Wextra %SOURCE_FILES% -o yum.exe
goto compiled
:msvc
cl.exe /nologo /std:c17 /TC /O2 /W4 /Fe:yum.exe %SOURCE_FILES%
:compiled
set RESULT=%errorlevel%
if not "%RESULT%"=="0" goto done
if "%NO_INSTALL%"=="1" goto done
if not exist "..\..\bin" mkdir "..\..\bin"
copy /y yum.exe "..\..\bin\yum.exe" >nul
set RESULT=%errorlevel%
goto done
:clean
del /q *.obj *.o yum.exe *.pdb *.ilk 2>nul
set RESULT=0
:done
popd
exit /b %RESULT%