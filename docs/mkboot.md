# mkboot

## What it does

Creates bootable Windows PE (WinPE) ISO images from a source directory or WIM file. It can also optionally service the mounted image by injecting drivers and startup commands before building the ISO.

## Usage

- `mkboot -s <SourceDir|WimFile> -o <Output.iso> [OPTIONS]`

## Options

- `-s, --source <path>`: required source directory or boot.wim file.
- `-o, --output <path>`: required output ISO path.
- `-b, --boot-file <path>`: custom El Torito boot file override.
- `-d, --driver <path>`: inject a driver package or directory; may be repeated.
- `--ignore-driver-errors`: continue when driver injection reports non-fatal errors.
- `-e, --exec <command>`: append a command to `startnet.cmd` during servicing.
- `-a, --arch <arch>`: target architecture (`x64`, `x86`, or `arm64`).
- `-l, --label <label>`: volume label for the ISO image.
- `-m, --mount-dir <path>`: explicit temporary mount directory for WIM servicing.
- `-f, --force`: overwrite an existing output file.
- `-v, --verbose`: enable detailed DISM/IMAPI logging.
- `-h, --help`: show help text.

## Build requirements

This tool is built against the Windows ADK/Deployment Tools DISM SDK.

Important: the Windows Assessment and Deployment Kit must be installed before building this tool. The build expects the DISM headers and libraries from the ADK, specifically the `DismApi` include and library directories used by the DISM SDK.

Without the ADK installed, the DISM API headers and libraries are not available, and the build cannot complete successfully.

For a normal end-user runtime install, the full ADK is not usually required just to execute a prebuilt binary. The ADK is a build-time dependency for the WinPE servicing features. If the target machine does not have the DISM runtime available, the servicing path will fail when it tries to mount or modify a WIM.

## Notes

- Requires a Windows environment with the Microsoft C++ toolchain.
- DISM-based servicing requires administrator privileges.
- IMAPI2 is used to generate the bootable ISO image structure.
- The tool is intended for WinPE image creation and servicing workflows on Windows hosts.
