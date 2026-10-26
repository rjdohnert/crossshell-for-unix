@echo off
setlocal EnableExtensions
set SOURCE_FILES=rg.c search_state.c allocation.c options.c unicode.c literal_matcher.c regex_matcher.c pattern_set.c printer.c text_reader.c path_conversion.c file_search.c glob_matcher.c ignore_rules.c directory_walker.c help.c command_line.c file_types.c multiline_search.c parallel_search.c
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
clang -std=c17 -O2 -Wall -Wextra %SOURCE_FILES% -o rg.exe
goto compiled
:msvc
cl.exe /nologo /std:c17 /TC /O2 /W4 /Fe:rg.exe %SOURCE_FILES%
:compiled
set RESULT=%errorlevel%
if not "%RESULT%"=="0" goto done
if "%NO_INSTALL%"=="1" goto done
if not exist "..\..\bin" mkdir "..\..\bin"
copy /y rg.exe "..\..\bin\rg.exe" >nul
set RESULT=%errorlevel%
goto done
:clean
del /q *.obj *.o rg.exe *.pdb *.ilk 2>nul
set RESULT=0
:done
popd
exit /b %RESULT%