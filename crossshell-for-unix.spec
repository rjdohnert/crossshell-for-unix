Name:          CrossShell-for-UNIX
Version:       15.7
Release:       1
Summary:       CrossShell for UNIX - 250 Native UNIX Utilities for Windows
License:       BSD-3-Clause
Group:         Applications/System
URL:           https://github.com/rjdohnert/cmd-extended
Vendor:        PC/OpenSystems LLC
Packager:      PC/OpenSystems LLC
BuildArch:     x86_64

%description
CrossShell for UNIX delivers 250 native, object-oriented reimplementations of
core UNIX and BSD utilities engineered specifically for modern Windows environments.
Installs directly to C:\Windows\System32.

%install
if not exist "%BUILDROOT%\Windows\System32" mkdir "%BUILDROOT%\Windows\System32"
copy /Y "bin\*.exe" "%BUILDROOT%\Windows\System32\"

%files
%attr(0755, SYSTEM, Administrators) "C:\Windows\System32\*.exe"
