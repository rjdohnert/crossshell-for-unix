<img width="500" height="210" alt="crossshell-github" src="https://github.com/user-attachments/assets/dc64eede-62b2-48e5-b696-0eac42902d55" />

# CrossShell for UNIX Open Source

[![Release](https://img.shields.io/badge/release-v15.7-blue.svg)](Release/)
[![License](https://img.shields.io/badge/license-BSD%203--Clause-green.svg)](LICENSE.txt)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Server-lightgrey.svg)](#supported-systems)
[![Architecture](https://img.shields.io/badge/arch-x64-orange.svg)](#installation)
=======

CrossShell for UNIX v15.7 Open Source 
------------
This is a collection of command line tools for the Windows command line based on the BSD coreutils and more.

**CrossShell for UNIX** is the open source release of the UNIX/Linux interoperability command-line environment from **PC/OpenSystems, LLC**.

This project delivers over 250 object-oriented UNIX inspired utilities reimagined for modern Windows environments—without pretending Windows is suddenly BSD, UNIX, or Linux. Built to seamlessly interface with deep Windows subsystems including the Windows Registry and CIM/WMI classes, CrossShell empowers system administrators, developers, lab environments, and infrastructure teams with a familiar, fluid UNIX command-line workflow while remaining 100% native on Windows.

---
Disclaimer
-----------
These tools are a reimplimination of common UNIX System V and BSD utilities that were created for interoperability for Windows systems
because of the structural differences between Windows and native POSIX systems it is near impossible to port the actual source to run
natively on Windows and this also reduces licensing issues with the GPL and official BSD sources.  Whenever source code was observed for
reimplementation actual BSD source code was observed but no official sources were used in the creation of these tools.  GNU source code was 
observed during the production of one of these utilities UFW.

GNU and the GPL are registered trademarks of The Free Software Foundation.  FreeBSD is a registered Trademark of
the FreeBSD foundation.  Linux is a registered trademark of Linus Torvalds.  Windows/Windows Server are registered trademarks of
Microsoft Corporation

## Table of Contents
=======

- [Overview](#overview)
- [Current Release](#current-release)
- [Why This Exists](#why-this-exists)
- [Project Highlights](#project-highlights)
  - [Standout Windows-Specific Utilities](#standout-windows-specific-utilities)
- [Documentation](#documentation)
- [Supported Systems](#supported-systems)
- [POSIX and Compatibility Notes](#posix-and-compatibility-notes)
- [Installation](#installation)
  - [GUI Installation](#gui-installation)
  - [Command-Line Installation](#command-line-installation)
  - [Silent Installation](#silent-installation)
  - [Portable Installation (ZIP)](#portable-installation-zip)
- [Uninstallation](#uninstallation)
- [Building Utilities](#building-utilities)
- [Project History](#project-history)
- [AI Use Disclosure](#ai-use-disclosure)
- [Acknowledgments](#acknowledgments)
- [License & Legal](#license--legal)

---

## Overview

CrossShell for UNIX delivers modern, object-oriented reimplementations of core UNIX utilities engineered specifically for the Windows platform. Rather than relying on rigid emulation layers, POSIX translation shims, or virtualization overhead, CrossShell tools interface directly with deep Windows architecture—including the **Windows Registry**, **CIM (Common Information Model)**, and **WMI (Windows Management Instrumentation)** classes.

This architecture brings the best of both worlds: the expressive ergonomics and brevity of traditional UNIX commands combined with the rich, structured system inspection capabilities of native Windows internals.

## Current Release

- **Version:** `15.7`
- **MSI Package:** `CrossShell-for-UNIX-15.7.msi`
- **Portable Archive:** `CrossShell-For-UNIX-bin.zip`

---

## Why This Exists

Windows environments frequently require powerful command-line tooling, yet many classic UNIX workflows that system administrators rely upon are not available in a consistent out-of-the-box format.

### Key Benefits

- **Everyday Administration:** Gives Windows administrators a robust UNIX-style toolbox for day-to-day operations.
- **Deep Windows Integration:** Object-oriented implementations interact natively with the Windows Registry, CIM/WMI providers, PnP manager, and Win32 subsystems.
- **Mixed-Environment Management:** Makes managing hybrid Windows and UNIX/Linux environments seamless and intuitive.
- **Workflow & Script Continuity:** Brings familiar command semantics, flags, and pipeline behavior to automation, troubleshooting, and support scripts.
- **100% Native Binaries:** Eliminates the overhead, translation latency, or virtualization requirements of VMs and POSIX subsystems.
- **Cross-Platform Compatibility:** Delivers practical compatibility shims for Linux, BSD, and traditional UNIX workflows.

> [!NOTE]
> CrossShell for UNIX is **not** a certified UNIX product by The Open Group, nor is it a UNIX distribution or a POSIX subsystem. It is not intended as a direct replacement for OpenBSD, AIX, Linux, or WSL2. If you require full POSIX standard compliance, WSL2 or a dedicated Linux/UNIX installation is recommended. If you want native Windows tools with authentic UNIX flavor without VM or compatibility layer overhead, CrossShell for UNIX is built for you.

---

## Project Highlights

- **Native KornShell (`ksh`):** A working, functional `ksh` reimplementation backed by sustained development history.
- **250 Utilities:** Extensive catalog of UNIX utilities including all of the BSD coreutils tailored for Windows.
- **Deep Documentation:** Fully documented commands across the [docs/](docs/) library and [command-reference.md](command-reference.md).
- **Windows-Focused Tooling:** Native administrative utilities custom-engineered specifically for the CrossShell for UNIX product suite.

### Standout Windows-Specific Utilities

In addition to classic UNIX utilities, CrossShell provides specialized tools designed specifically for Windows administration:

| Utility | Description |
| :--- | :--- |
| `fsctl` | Filesystem control shell and interactive maintenance tooling. |
| `vcc` | Visual Studio C++ compiler wrapper that automatically detects the MSVC environment (even when not in `PATH`) and accepts GCC/POSIX-style flags. |
| `netctl` | Packet capture and network traffic analysis with Winsock and optional Npcap support. |
| `vhdctl` | Virtual hard disk (VHD/VHDX) inspection, mounting, and control tooling. |
| `dskctl` | Disk control, drive inspection, and cleanup utility built for Windows maintenance workflows. |
| `pthctl` | System and user `PATH` environment inspection, normalization, cleanup, and management. |
| `registryctl` | Windows Registry inspection, navigation, and value management from the command line. |
| `driverctl` | Plug and Play (PnP) device enumeration, driver stack analysis, and hardware diagnostic health reporting. |
| `handles` | Process open handle count inspection, filtering, and resource tracking. |
| `threads` | Process thread count inspection, diagnostics, and concurrency monitoring. |

---

## Documentation

Comprehensive project documentation is structured into two main tiers:

- **[command-reference.md](command-reference.md)**: Fast overview and index of all available tools categorized by function.
- **[docs/](docs/)**: Complete per-command reference pages containing in-depth usage manuals, option flags, and real-world examples.

> [!TIP]
> - For a quick lookup of command names and summaries, refer to the **[Command Reference](command-reference.md)**.
> - For detailed syntax, argument flags, and examples, explore the **[Docs Directory](docs/)**.

---

## Supported Systems

CrossShell for UNIX is built and tested for 64-bit (x64) Windows operating systems across both enterprise and desktop tiers:

### Windows Server
- Windows Server 2025 (Desktop Experience & Server Core)
- Windows Server 2022 (Desktop Experience & Server Core)

### Windows Client
- Windows 11 (Enterprise, Pro, Home, and Education)
- Windows 10 (Enterprise, Pro, Home, and Education)

---

## POSIX and Compatibility Notes

CrossShell for UNIX does not attempt to turn Windows into a strictly POSIX-compliant operating system. 

Windows and UNIX architectures are fundamentally distinct (e.g., file locking, access control lists, process models, and path separators). CrossShell bridges this gap through a pragmatic approach:
- Some commands are **faithful in spirit and behavior**.
- Some commands are **direct functional equivalents**.
- Some commands are **object-oriented Windows-native reinterpretations** engineered to operate directly against the Windows Registry, CIM/WMI providers, and Win32 APIs, solving classic UNIX administration problems using idiomatic Windows paradigms.

This balance ensures maximum practical utility while preserving native Windows performance and reliability.

---

## Installation

Installers are packaged as standard Microsoft Windows Installer (`.msi`) packages and are located in the [Release/](Release/) directory as well as GitHub Releases.

### GUI Installation

1. Navigate to the `Release/` directory.
2. Right-click `CrossShell-for-UNIX-15.7.msi` and select **Run as administrator**.
3. Accept the User Account Control (UAC) prompt.
4. Accept the BSD 3-Clause license agreement and follow the setup wizard to complete installation.

### Command-Line Installation

Open an elevated Command Prompt or PowerShell terminal and run:

```cmd
msiexec /i "Release\CrossShell-for-UNIX-15.7.msi"
```

### Silent Installation

To perform an unattended deployment with detailed logging:

```cmd
msiexec /i "Release\CrossShell-for-UNIX-15.7.msi" /qn /L*V "Release\install.log"
```

### Portable Installation (ZIP)

The `.zip` file in the [Release/](Release/) directory (`CrossShell-For-UNIX-bin.zip`) can be used for portable installation without running an installer:

1. Decompress the `.zip` file to any folder of your choice (e.g., `C:\Tools\CrossShell` or `D:\CrossShell\bin`).
2. Add the decompressed directory containing the `.exe` files to your `PATH` environment variable (either System `PATH` or User `PATH`).
3. Open a new Command Prompt, PowerShell, or Windows Terminal window to begin using all 250 CrossShell commands immediately.

### RPM Package (RPM)

An RPM package is provided for users who used a previous release or the beta release of this product

---

## Uninstallation

To uninstall CrossShell for UNIX:
1. Open Windows **Settings** (`Win + I`).
2. Navigate to **Apps** > **Installed apps** (or **Apps & features**).
3. Locate **CrossShell for UNIX** and select **Uninstall**.

---

## Building Utilities

`realpath`, `reboot`, `recode`, `recycle`, `refresh`, `registryctl`, `rename`,
`renice`, `resolve`, `rm`, `rpm`, `rpmbuild`, `split`, `ss`, `startsrc`, `stat`,
`stdbuf`, `stop`, `stopsrc`, `strace`, `strings`, `su`, `sudo`, `supervisord`,
`suspend`, `swlist`, `sync`, `tail`, `tee`, `test`, `threads`, `tmux`, `top`,
`touch`, `tr`, `traceroute`, `true`, `truncate`, `ts`, `tsort`, `tty`,
`typeset`, `ufw`, `ulimit`, `umask`, `uname`, `uniq`, `unxz`, `unzip`,
`uptime`, `useradd`, `userdel`, `usermod`, `vcc`, `vhdctl`, `vmstat`, `w`,
`wall`, `watch`, `wc`, `wget`, `whence`, `whereami`, `whereis`, `which`, `who`,
`whois`, `wipe`, `xargs`, `xorriso`, `xz`, `yacc`, `yes`, `zcat`, `zgrep`, and `zip`
have function-named modules in
their respective `src/<command>/` directories. Each directory includes
`build.ps1`, `build.bat`, and `build.sh`, which compile all of that command's
implementation files. Compile the complete module set rather than just the
entry-point source file.

Use a Windows C++17 toolchain: Clang or MSVC with the Windows SDK, or MinGW-w64
GCC. MSVC requires a Developer Command Prompt or an initialized developer
environment. The shell scripts run in a Windows shell environment such as Git
Bash; they do not port the Windows APIs to Linux.

From the repository root, for example:

```powershell
./src/realpath/build.ps1 -NoInstall
./src/realpath/build.ps1 -Compiler cl -NoInstall
./src/realpath/build.ps1 -Clean
```

```cmd
src\realpath\build.bat --no-install
src\realpath\build.bat --clean
```

```sh
sh src/realpath/build.sh --no-install
sh src/realpath/build.sh --clean
```

The scripts can be invoked from any working directory. By default, they also
copy the resulting executable to the project's `bin/` directory; use
`-NoInstall` (PowerShell) or `--no-install` (batch/shell) to skip that step.
Cleaning only removes local build artifacts, not installed binaries.

---

## Project History

CrossShell for UNIX has been in active development since 2023.

The broader reimplementation effort was restarted in 2023 with the objective of creating a serious, native Windows toolbox that feels second nature to experienced UNIX users while fully respecting Windows architecture.

The `ksh` (KornShell) component has an even longer heritage:
- **2008:** Development began as an internal project for an enterprise client requiring a specialized `ksh` scripting engine for their backend services.
- **2023:** Following the client's dissolution, the codebase was maintained and actively resumed as the flagship shell for the CrossShell for UNIX initiative.
- **Present:** `ksh` stands as a core centerpiece of the platform, reflecting years of dedicated optimization for authentic behavior and daily usability.

This open source release consolidates years of accumulated engineering, experimentation, rewrites, and refinement developed within **PC/OpenSystems, LLC**.

---

## AI Use Disclosure

These tools were designed and developed primarily by the engineering team at PC/OpenSystems LLC. AI assistance was utilized for historical reference, documentation support, and security review. 

All project direction, architectural decisions, core implementations, and product identity remain strictly human-led. This codebase contains approximately 30% AI-assisted code.

---

## Acknowledgments

Special thanks and appreciation to:

- The **CrossShell for UNIX** development and testing team at PC/OpenSystems, LLC
- Members of the **IBM AIX** team
- Members of the **OpenBSD** community

Their guidance, feedback, testing, and assistance in preserving authentic look, feel, and ergonomics have been instrumental throughout development.

---

## License & Legal

### License

CrossShell for UNIX is released under the **[BSD 3-Clause License](LICENSE.txt)**.

### Copyright Notice

```text
Copyright (c) 2023-2026 PC/OpenSystems LLC, and contributors. All rights reserved.
```

### Trademarks

- **UNIX** is a registered trademark of The Open Group.
- **OpenBSD** is a registered trademark of Theo de Raadt.
- **IBM** and **AIX** are trademarks of International Business Machines Corporation.
- **Windows** and **Windows Server** are registered trademarks of Microsoft Corporation.
- All other trademarks and registered trademarks are the property of their respective owners.
