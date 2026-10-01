# Building RPM Packages for Windows (WinRPM Guide)

Welcome to the comprehensive guide for creating RPM packages on Microsoft Windows using **RPM** (`rpmbuild` and `rpm`). Whether you are packaging a simple command-line script, a compiled binary utility, or an enterprise Windows background service, this guide walks you through every step in a clear, beginner-friendly way.

---

## Table of Contents

1. [Introduction to Windows RPM](#1-introduction-to-windows-rpm)
2. [Prerequisites & Tools](#2-prerequisites--tools)
3. [Understanding the RPM Workflow](#3-understanding-the-rpm-workflow)
4. [Anatomy of a Windows SPEC File](#4-anatomy-of-a-windows-spec-file)
   - [Header Tags](#header-tags)
   - [Macros & Paths](#macros--paths)
   - [Build Stages (`%prep`, `%build`, `%install`)](#build-stages)
   - [Lifecycle Scripts (`%pre`, `%post`, `%preun`, `%postun`)](#lifecycle-scripts)
   - [The Files Section (`%files`)](#the-files-section-files)
5. [Step-by-Step Tutorial: Your First Windows RPM](#5-step-by-step-tutorial-your-first-windows-rpm)
6. [Real-World Examples](#6-real-world-examples)
   - [Example A: Simple CLI Tool](#example-a-simple-cli-tool)
   - [Example B: Windows Background Service](#example-b-windows-background-service)
   - [Example C: Application with Configuration & Docs](#example-c-application-with-configuration--docs)
7. [Building the Package (`rpmbuild`)](#7-building-the-package-rpmbuild)
8. [Testing & Managing the Package (`rpm`)](#8-testing--managing-the-package-rpm)
9. [Best Practices & Troubleshooting](#9-best-practices--troubleshooting)
10. [Command Quick Reference](#10-command-quick-reference)

---

## 1. Introduction to Windows RPM

RPM (Red Hat Package Manager) is one of the most widely used enterprise software packaging formats. **RPM** for Windows brings native RPM packaging and deployment to Microsoft Windows (Windows 10, 11, and Windows Server).

### Key Benefits on Windows:
- **Single-File Distribution**: Ship executables, DLLs, configs, and assets in a single `.rpm` archive.
- **Cryptographic Verification**: Every file has a calculated SHA-256 digest tracked in the RPM database.
- **Lifecycle Management**: Clean upgrades (`rpm -U`), uninstalls (`rpm -e`), and integrity checking (`rpm -V`).
- **Transactional Safety**: Write-Ahead Logging (WAL) ensures atomic file installation and rollback on failure.

---

## 2. Prerequisites & Tools

Make sure the following utilities from `cmd-extended` are available in your `PATH` or working directory:

1. **`rpmbuild.exe`**: The package compiler that parses `.spec` files, stages files, and builds `.rpm` archives.
2. **`rpm.exe`**: The package manager that queries, installs, verifies, and uninstalls `.rpm` packages.

Verify tool availability:
```powershell
rpmbuild --version
rpm --version
```

---

## 3. Understanding the RPM Workflow

```
+-------------------------------------------------------------+
| 1. Write the SPEC File (.spec)                              |
|    Defines package metadata, build steps, scripts, files    |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
| 2. Run rpmbuild -bb <file.spec>                             |
|    - Runs %prep, %build, and %install stages                |
|    - Copies files into the temporary BUILDROOT directory     |
|    - Generates SHA-256 checksums                            |
|    - Creates yourpackage-1.0.0-1.x86_64.rpm                 |
+-------------------------------------------------------------+
                              |
                              v
+-------------------------------------------------------------+
| 3. Distribute and Install with rpm -ivh <file.rpm>          |
|    - Validates dependencies                                 |
|    - Executes %pre script                                   |
|    - Unpacks files into target folders (e.g. Program Files) |
|    - Executes %post script                                  |
|    - Records metadata in C:\ProgramData\RPM\rpmdb.json      |
+-------------------------------------------------------------+
```

---

## 4. Anatomy of a Windows SPEC File

A `.spec` file is a plain text recipe that tells `rpmbuild` how to create your package.

### Header Tags
These directives describe basic metadata about your software:

| Tag | Purpose | Example |
| :--- | :--- | :--- |
| `Name:` | Package name (lowercase, no spaces) | `enterprise-tool` |
| `Version:` | Software release version (SemVer) | `1.2.0` |
| `Release:` | Package build counter (increments on repackage) | `1` |
| `Summary:` | Short one-line synopsis | `High performance log analyzer` |
| `License:` | Software license | `MIT`, `Apache-2.0`, `Proprietary` |
| `Group:` | Category of software | `Applications/System`, `Development/Tools` |
| `URL:` | Project website or documentation link | `https://github.com/myorg/tool` |
| `Vendor:` | Company or author name | `Acme Corp` |
| `BuildArch:` | Target architecture | `x86_64` or `arm64` |
| `Requires:` | Prerequisite packages or capabilities | `PowerShell >= 5.1`, `VCRedist >= 14.0` |
| `Provides:` | Virtual capabilities provided | `log-parser` |
| `Conflicts:` | Packages incompatible with this one | `old-log-tool < 2.0` |

---

### Macros & Paths
Macros are reusable path variables that make your spec portable:

| Macro | Default Windows Path |
| :--- | :--- |
| `%{name}` | The value of the `Name:` tag |
| `%{version}` | The value of the `Version:` tag |
| `%{release}` | The value of the `Release:` tag |
| `%{buildroot}` | Staging area: `C:\Users\<User>\AppData\Local\Temp\BUILDROOT_...` |
| `%{_prefix}` | `C:\Program Files` |
| `%{_bindir}` | `C:\Program Files\%{name}\bin` |
| `%{_sysconfdir}` | `C:\ProgramData\%{name}\config` |
| `%{_datadir}` | `C:\Program Files\%{name}\share` |
| `%{_defaultdocdir}`| `C:\Program Files\%{name}\doc` |

You can also define custom macros at the top of your spec:
```spec
%define myinstallpath C:\Tools\%{name}
```

---

### Build Stages
These sections contain batch or command-line commands run by `rpmbuild` during compilation:

* **`%prep`**: Unpack source tarballs or extract source assets.
* **`%build`**: Compile the code (e.g. invoke `cl.exe`, `cmake`, `msbuild`, `go build`, or `dotnet build`).
* **`%install`**: Stage files into the `BUILDROOT` directory.
  > **Important**: Files MUST be staged under `%BUILDROOT%` (or `%RPM_BUILD_ROOT%`) exactly where they should be installed on the destination computer.
* **`%clean`**: Clean up temporary build artifacts.

---

### Lifecycle Scripts
These scripts execute automatically on the user's computer during package operations:

* **`%pre`**: Runs **before** files are installed.
* **`%post`**: Runs **after** files are installed (ideal for service creation, registry keys, or shortcuts).
* **`%preun`**: Runs **before** uninstallation (ideal for stopping services or unregistering tasks).
* **`%postun`**: Runs **after** uninstallation (ideal for clearing runtime logs or caches).

---

### The Files Section (`%files`)
Lists every file and directory that belongs in the package:

* **`%attr(mode, user, group)`**: Assigns POSIX octal permissions (e.g. `0755` for executables, `0644` for data files).
* **`%config`**: Marks a configuration file.
* **`%config(noreplace)`**: Protects a configuration file so upgrades will NOT overwrite local user modifications.
* **`%doc`**: Marks documentation files (README, LICENSE, etc.).

Example:
```spec
%files
%attr(0755, SYSTEM, Administrators) "C:\Program Files\mytool\bin\mytool.exe"
%config(noreplace) "C:\ProgramData\mytool\config\settings.json"
%doc "C:\Program Files\mytool\doc\README.md"
```

---

## 5. Step-by-Step Tutorial: Your First Windows RPM

Let's build a practical package called `hello-win` from scratch.

### Step 1: Create a Project Workspace
Create a folder structure for your project:
```powershell
mkdir C:\Packaging\hello-win
cd C:\Packaging\hello-win
mkdir src
```

### Step 2: Create Application Files
Create your source script/binary in `src\hello.bat`:
```bat
@echo off
echo Hello from RPM on Windows!
echo Current time: %TIME%
```

Create a sample configuration file in `src\hello.conf`:
```ini
[Settings]
Greeting=Hello World
LogLevel=Info
```

Create a documentation file in `src\README.txt`:
```
hello-win Version 1.0.0
A sample utility packaged with RPM for Windows.
```

---

### Step 3: Write the SPEC File (`hello-win.spec`)
In `C:\Packaging\hello-win\hello-win.spec`, write:

```spec
Name:          hello-win
Version:       1.0.0
Release:       1
Summary:       Sample Hello World utility for Windows
License:       MIT
Group:         Applications/Utilities
URL:           https://example.com/hello-win
BuildArch:     x86_64

%description
hello-win is a starter utility designed to demonstrate native
Windows RPM package creation and deployment.

%install
REM Create directories inside BUILDROOT
mkdir "%BUILDROOT%\Program Files\hello-win\bin"
mkdir "%BUILDROOT%\ProgramData\hello-win\config"
mkdir "%BUILDROOT%\Program Files\hello-win\doc"

REM Copy staged files into place
copy /Y "src\hello.bat" "%BUILDROOT%\Program Files\hello-win\bin\"
copy /Y "src\hello.conf" "%BUILDROOT%\ProgramData\hello-win\config\"
copy /Y "src\README.txt" "%BUILDROOT%\Program Files\hello-win\doc\"

%post
echo [hello-win] Installation complete! Access hello.bat from 'C:\Program Files\hello-win\bin\'.

%preun
echo [hello-win] Preparing to remove package...

%files
%attr(0755, SYSTEM, Administrators) "C:\Program Files\hello-win\bin\hello.bat"
%config(noreplace) "C:\ProgramData\hello-win\config\hello.conf"
%doc "C:\Program Files\hello-win\doc\README.txt"
```

---

### Step 4: Build the RPM Package
Run `rpmbuild`:
```powershell
rpmbuild -bb hello-win.spec
```

**Output:**
```
Executing build for package: hello-win-1.0.0-1
Processing %files section and calculating cryptographic SHA-256 digests...
  File: Program Files/hello-win/bin/hello.bat [SHA256: 4a2b1c... Mode: 100755]
  File: ProgramData/hello-win/config/hello.conf [SHA256: 8f9e0a... Mode: 100644]
  File: Program Files/hello-win/doc/README.txt [SHA256: 3c1d2e... Mode: 100644]
Wrote: C:\Packaging\hello-win\hello-win-1.0.0-1.x86_64.rpm (1536 bytes)
```

Congratulations! You have built your first Windows RPM package!

---

## 6. Real-World Examples

### Example A: Simple CLI Tool

```spec
Name:          disk-checker
Version:       2.1.0
Release:       1
Summary:       Fast NTFS volume health and disk usage inspection tool
License:       Apache-2.0
Group:         Applications/System
BuildArch:     x86_64
Requires:      PowerShell >= 5.1

%description
disk-checker is a lightweight command-line tool that inspects
NTFS volume statistics and generates storage usage alerts.

%install
mkdir "%BUILDROOT%\Program Files\disk-checker\bin"
copy /Y "bin\diskchk.exe" "%BUILDROOT%\Program Files\disk-checker\bin\"

%files
%attr(0755, SYSTEM, Administrators) "C:\Program Files\disk-checker\bin\diskchk.exe"
```

---

### Example B: Windows Background Service

This example shows how to automatically register and start a Windows service on installation (`%post`) and stop/delete it on removal (`%preun`):

```spec
Name:          telemetry-agent
Version:       3.4.0
Release:       1
Summary:       Enterprise Telemetry Collection Daemon for Windows Server
License:       Proprietary
Group:         System/Daemons
BuildArch:     x86_64

%description
Background monitoring agent that continuously streams system metrics to
centralized telemetry collectors.

%install
mkdir "%BUILDROOT%\Program Files\telemetry-agent\bin"
mkdir "%BUILDROOT%\ProgramData\telemetry-agent\config"
copy /Y "bin\agent.exe" "%BUILDROOT%\Program Files\telemetry-agent\bin\"
copy /Y "config\agent.yaml" "%BUILDROOT%\ProgramData\telemetry-agent\config\"

%post
echo Registering TelemetryAgent Windows Service...
sc.exe create TelemetryAgent binPath= "C:\Program Files\telemetry-agent\bin\agent.exe" start= auto
sc.exe start TelemetryAgent

%preun
echo Stopping and deleting TelemetryAgent Service...
sc.exe stop TelemetryAgent
sc.exe delete TelemetryAgent

%files
%attr(0755, SYSTEM, Administrators) "C:\Program Files\telemetry-agent\bin\agent.exe"
%config(noreplace) "C:\ProgramData\telemetry-agent\config\agent.yaml"
```

---

### Example C: Application with Configuration & Docs

```spec
Name:          web-forwarder
Version:       1.0.4
Release:       2
Summary:       Reverse proxy and port forwarder for IIS and Kestrel
License:       MIT
Group:         Networking/Daemons
BuildArch:     x86_64

%description
High throughput HTTP/HTTPS traffic redirector and reverse proxy.

%install
mkdir "%BUILDROOT%\Program Files\web-forwarder\bin"
mkdir "%BUILDROOT%\ProgramData\web-forwarder\config"
mkdir "%BUILDROOT%\Program Files\web-forwarder\doc"

copy /Y "forwarder.exe" "%BUILDROOT%\Program Files\web-forwarder\bin\"
copy /Y "routes.json" "%BUILDROOT%\ProgramData\web-forwarder\config\"
copy /Y "docs\*" "%BUILDROOT%\Program Files\web-forwarder\doc\"

%files
%attr(0755, SYSTEM, Administrators) "C:\Program Files\web-forwarder\bin\forwarder.exe"
%config(noreplace) "C:\ProgramData\web-forwarder\config\routes.json"
%doc "C:\Program Files\web-forwarder\doc\*"
```

---

## 7. Building the Package (`rpmbuild`)

### Common Build Commands

* **Build binary package (standard)**:
  ```powershell
  rpmbuild -bb package.spec
  ```

* **Build both binary and source RPMs**:
  ```powershell
  rpmbuild -ba package.spec
  ```

* **Test the `%prep` stage only**:
  ```powershell
  rpmbuild -bp package.spec
  ```

* **Test the `%install` stage only**:
  ```powershell
  rpmbuild -bi package.spec
  ```

* **Check the `%files` list against staged files**:
  ```powershell
  rpmbuild -bl package.spec
  ```

* **Specify an explicit output filename**:
  ```powershell
  rpmbuild -bb package.spec --output=MyCustomApp.rpm
  ```

* **Define or override macros on the command line**:
  ```powershell
  rpmbuild -bb package.spec --define="version 2.0.0" --define="release 5"
  ```

* **Automatically clean the staging tree after building**:
  ```powershell
  rpmbuild -bb package.spec --clean
  ```

---

## 8. Testing & Managing the Package (`rpm`)

Once you have generated your `.rpm` file, use `rpm.exe` to manage it:

### Querying Uninstalled Package Files (`-qp`)
* **Display package metadata (name, version, license, description)**:
  ```powershell
  rpm -qip hello-win-1.0.0-1.x86_64.rpm
  ```
* **List all files contained inside the RPM**:
  ```powershell
  rpm -qlp hello-win-1.0.0-1.x86_64.rpm
  ```

---

### Installing Packages (`-i`)
*(Requires Administrator privileges)*

* **Install package with progress indicators (`-h`) and verbose output (`-v`)**:
  ```powershell
  rpm -ivh hello-win-1.0.0-1.x86_64.rpm
  ```

* **Test installation without making actual changes (`--test`)**:
  ```powershell
  rpm -i --test hello-win-1.0.0-1.x86_64.rpm
  ```

* **Install to a custom root directory (`--root`)**:
  ```powershell
  rpm -ivh hello-win-1.0.0-1.x86_64.rpm --root="D:\InstalledApps"
  ```

---

### Querying Installed Packages (`-q`)
* **List all installed RPM packages on the machine**:
  ```powershell
  rpm -qa
  ```

* **View detailed information about an installed package**:
  ```powershell
  rpm -qi hello-win
  ```

* **List all files installed by a package**:
  ```powershell
  rpm -ql hello-win
  ```

---

### Verifying File Integrity (`-V`)
Checks installed files against their cryptographic SHA-256 digests stored in the RPM database:

* **Verify a single package**:
  ```powershell
  rpm -V hello-win
  ```
* **Verify all installed packages across the entire system**:
  ```powershell
  rpm -Va
  ```

* **Interpretation of Output**:
  * Clean (no output): All files match their original installation state.
  * `..5...... <file>`: File content has changed (SHA-256 digest mismatch).
  * `missing   <file>`: File was deleted or moved.

---

### Upgrading Packages (`-U`)
Installs the new version and safely cleans up old files while preserving modified `%config(noreplace)` files:
```powershell
rpm -Uvh hello-win-1.1.0-1.x86_64.rpm
```

---

### Uninstalling Packages (`-e`)
Executes `%preun`, removes all packaged files, executes `%postun`, and clears the RPM database entry:
```powershell
rpm -ev hello-win
```

---

## 9. Best Practices & Troubleshooting

### Best Practices:
1. **Always use standard Windows paths** in the `%files` section (e.g. `C:\Program Files\<name>` for binaries and `C:\ProgramData\<name>` for configs).
2. **Protect User Configs**: Use `%config(noreplace)` for `.ini`, `.json`, or `.yaml` files so upgrades do not wipe out user settings.
3. **Set Permissions**: Mark executables with `%attr(0755, SYSTEM, Administrators)` and data/configs with `%attr(0644, SYSTEM, Administrators)`.
4. **Use Administrative Shell**: Run PowerShell or Command Prompt as **Administrator** when running `rpm -i`, `rpm -U`, or `rpm -e`.

### Troubleshooting:

* **`error: BUILDROOT directory does not exist`**:
  * Make sure your `%install` script creates directories using `mkdir "%BUILDROOT%\..."` before copying files into them.
* **`error: Failed dependencies`**:
  * If a required capability is missing, either install the dependency or use `--nodeps` for testing:
    ```powershell
    rpm -ivh package.rpm --nodeps
    ```
* **`error: package is already installed`**:
  * Use `-U` (upgrade) instead of `-i` (install), or erase the old version with `rpm -e <name>`.

---

## 10. Command Quick Reference

### `rpmbuild` Quick Reference
```powershell
# Build binary RPM package
rpmbuild -bb <file.spec>

# Build both binary and source RPMs
rpmbuild -ba <file.spec>

# Override version and release
rpmbuild -bb <file.spec> --define="version 2.0" --define="release 1"

# Specify custom output path
rpmbuild -bb <file.spec> --output=MyPackage.rpm
```

### `rpm` Quick Reference
```powershell
# Query uninstalled package metadata and files
rpm -qip <file.rpm>
rpm -qlp <file.rpm>

# Install package with progress bar
rpm -ivh <file.rpm>

# Upgrade package
rpm -Uvh <file.rpm>

# Verify on-disk file checksums
rpm -V <package_name>
rpm -Va

# Uninstall package
rpm -ev <package_name>
```
