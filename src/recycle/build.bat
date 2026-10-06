@echo off
setlocal EnableExtensions
set TOOL_NAME=recycle
set SOURCE_FILES=console_prompter.cpp option_parser.cpp output_formatter.cpp recycle_app.cpp recycle_engine.cpp recycle.cpp wide_pipe_buffer.cpp
set NO_INSTALL=0
set CLEAN=0
:parse
if "%~1"=="" goto configure
if /i "%~1"=="clean" goto set_clean
if /i "%~1"=="--clean" goto set_clean
if /i "%~1"=="-Clean" goto set_clean
if /i "%~1"=="--no-install" goto set_no_install
if /i "%~1"=="-NoInstall" goto set_no_install
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
where clang++ >nul 2>nul
if not errorlevel 1 goto clang
where cl.exe >nul 2>nul
if not errorlevel 1 goto msvc
where g++ >nul 2>nul
if not errorlevel 1 goto gcc
echo No suitable C++ compiler found (clang++, cl.exe, or g++ required).
set RESULT=1
goto done
:clang
clang++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -o "%TOOL_NAME%.exe" %SOURCE_FILES% -lshell32
goto compiled
:msvc
cl.exe /nologo /std:c++17 /O2 /EHsc /DWIN32_LEAN_AND_MEAN /DNOMINMAX "/Fe:%TOOL_NAME%.exe" %SOURCE_FILES% shell32.lib
goto compiled
:gcc
g++ -std=c++17 -O2 -DWIN32_LEAN_AND_MEAN -DNOMINMAX -municode -o "%TOOL_NAME%.exe" %SOURCE_FILES% -lshell32
:compiled
set RESULT=%errorlevel%
if not "%RESULT%"=="0" goto done
if "%NO_INSTALL%"=="1" goto done
if not exist "..\..\bin" mkdir "..\..\bin"
copy /y "%TOOL_NAME%.exe" "..\..\bin\%TOOL_NAME%.exe" >nul
set RESULT=%errorlevel%
goto done
:clean
del /q *.obj *.o "%TOOL_NAME%.exe" *.pdb *.ilk 2>nul
set RESULT=0
:done
popd
exit /b %RESULT%
