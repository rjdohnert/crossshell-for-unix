# pkg

## What it does
Wrapper for winget.  Also can be used to install local .msi/.msix packages.  Manages software packages and package-related operations.

## Commands
- `pkg install <package>`: install a package via `winget install`
- `pkg add <package>`: alias for install
- `pkg install --local <file.msi|file.msix>`: install a local MSI/MSIX package file
- `pkg delete <package>` / `pkg remove <package>` / `pkg rm <package>`: uninstall a package via `winget uninstall`
- `pkg search <query>`: search packages via `winget search`
- `pkg upgrade`: upgrade all packages via `winget upgrade --all`
- `pkg upgrade <package>`: upgrade a specific package
- `pkg info` / `pkg list`: show installed packages via `winget list`
- `pkg info <package>` / `pkg list <package>`: show package details via `winget show`
- `pkg update`: refresh package sources via `winget source update`

## Options
- `-h`, `--help`: show help text
- `-V`, `--version`: show version information

## Local install behavior
- `--local` is supported with `install`/`add` and expects exactly one file path.
- `.msi` files are installed using `msiexec /i <path>`.
- `.msix` files are installed using `Add-AppxPackage -Path <path>`.

## Examples
- `pkg install Git.Git`
- `pkg install --local C:\installers\tool.msi`
- `pkg install --local C:\installers\app.msix`

## UNIX origin
A package-management utility associated with BSD rather than one specific historic Unix release.
