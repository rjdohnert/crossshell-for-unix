@echo off
setlocal EnableExtensions
set TOOL_NAME=xmic
set SOURCE_FILES=capture_process_output.cpp command_line_helpers.cpp execute_ram_status_summary.cpp execute_virtual_switch_query.cpp format_wmi_output.cpp format_wmi_values.cpp parse_configuration.cpp print_help.cpp run_interactive_shell.cpp run_wsl_detection_self_tests.cpp ssh_transport.cpp winrm_transport.cpp wmi_connection.cpp wmi_queries.cpp wsl_detection.cpp xmic.cpp xmic_state.cpp
set NO_INSTALL=0
set CLEAN=0
:parse
if "%~1"=="" goto configure
if /i "%~1"=="clean" goto set_clean
if /i "%~1"=="--clean" goto set_clean
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
where clang++ >nul 2>nul
if not errorlevel 1 goto clang
where cl.exe >nul 2>nul
if not errorlevel 1 goto msvc
echo A Windows C++17 compiler with WMI/MI SDK libraries is required.
set RESULT=1
goto done
:clang
clang++ -std=c++17 -O2 -o "%TOOL_NAME%.exe" %SOURCE_FILES%
goto compiled
:msvc
cl.exe /nologo /std:c++17 /O2 /EHsc /W4 "/Fe:%TOOL_NAME%.exe" %SOURCE_FILES%
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