# yum

## What it does
Yellowdog Updater, Modified for Windows NT Provides YUM-style commands for native 
Windows package and system-update operations. Package commands dispatch to `winget.exe`; 
Windows Update commands use the Windows Update Agent COM API directly. This is not the Linux 
YUM/RPM implementation, and it does not require a UNIX shell.

## Usage
```text
yum [OPTIONS] COMMAND [ARGUMENTS...]
```

## Commands
- `install PACKAGE`: install an exact winget package match.
- `remove PACKAGE`, `erase PACKAGE`: uninstall an exact winget package match.
- `search QUERY`: search winget package sources.
- `info PACKAGE`: show an exact package match through winget.
- `list`: list installed winget-managed packages.
- `update`: query and list pending Windows software updates; does not download or install them.
- `sys-upgrade`: download and install pending, non-hidden Windows software updates.
- `upgrade`: upgrade all winget-managed packages, including packages with unknown installed versions; does not install Windows Update updates.
- `repolist`, `repo list`: list winget package sources.
- `repo add NAME URL`: register a winget source.
- `repo remove NAME`, `repo rm NAME`: remove a winget source.
- `help`: display the command menu.

## Options
- `-y`, `--assumeyes`: skip the `sys-upgrade` confirmation, including acceptance of applicable update licenses. For package installation and upgrade, request winget silent mode.
- `-v`, `--verbose`: log the winget command line and additional Windows Update result information.
- `-h`, `--help`: display help.
- `--version`: display version information.

Put global options before the command. `sys-upgrade` also accepts `-y`, `-v`,
their long forms, and help options after the command. Other commands do not
provide general trailing-option parsing.

## Windows Update Behavior
The commands have different purposes:

```powershell
yum update
yum sys-upgrade
yum upgrade
```

`update` is a query only. `sys-upgrade` uses the configured Windows Update
service and respects its policies, including managed update-service settings.
It searches for software updates that are not installed and not hidden; driver
updates are not selected by this command.

Run `sys-upgrade` from an Administrator terminal. YUM does not elevate itself.
It lists pending updates and displays unaccepted license terms. Unless `-y` is
supplied, it asks for confirmation before accepting licenses, downloading, or
installing updates. Declining cancels the operation.

The operation downloads updates, verifies which ones are available locally,
and installs the downloaded subset. Download and installation failures are
reported; successfully installed updates are listed individually. A pending
restart can prevent installation. YUM reports restart requirements but never
reboots the system automatically.

## Examples
```powershell
yum search Git
yum info Git.Git
yum install Git.Git
yum remove Git.Git
yum list
yum upgrade
yum update
```

From an Administrator terminal:

```powershell
yum sys-upgrade
yum -y sys-upgrade
yum sys-upgrade -v
```

Package-source management:

```powershell
yum repolist
yum repo add MySource https://example.com/winget
yum repo remove MySource
```

Adding or removing sources and installing/removing software changes the system.
Use `yum sys-upgrade --help` to inspect the new command without running updates.

## Exit Status
For `sys-upgrade`:
- `0`: successful completion, no pending updates, or cancellation before download/installation.
- `1`: elevation, search, download, installation, or other processing failure. Some updates may have installed successfully before a partial failure.
- `3010`: a restart is required after successful installation, or before installation can proceed. Restart manually.

Package commands return the dispatched winget process's exit code. Command-line
errors generally return `1`; help and version requests return `0`.

## Requirements
- Native Windows with Windows Update Agent available and its configured service accessible.
- Administrator privileges for `sys-upgrade`.
- `winget.exe` available for package and repository commands. Windows Update commands do not depend on winget.

## Building
Using an initialized Windows C toolchain, from the repository root:

```powershell
.\src\yum\build.ps1 -NoInstall
.\src\yum\build.ps1 -Compiler cl -NoInstall
```

The implementation is split into function-named C17 modules for argument
escaping, winget dispatch, package/repository operations, Windows Update
query/install, privilege checking, help, and command-line dispatch. Compile
the complete module set rather than the entry-point `yum.c` alone.

The original monolithic source is retained byte-for-byte as
`src/yum/yum.c.orig` and is not compiled by the modular build. The scripts use
only `clang` or `cl` and the Windows SDK/C runtime. Linker directives select
the Windows COM and security libraries; no external libraries, package
managers, or generated build systems are required.

PowerShell builds install to `bin/yum.exe` by default; `-NoInstall` leaves the
binary in the source folder. `build.bat` and `build.sh` are also provided and
accept `--no-install` and `--clean`. PowerShell uses `-Clean`. Cleaning leaves
`yum.c.orig` and installed binaries intact. Shell-script compilation requires
a Windows toolchain, not a Linux or UNIX runtime for the program.