# wslmgr

## What it does

Provides an interactive Windows Subsystem for Linux manager for installed distributions, the online distribution catalog, imports, diagnostics, lifecycle operations, and containers managed by Microsoft's `wslc` CLI.

## Usage

```text
wslmgr
```

`wslmgr` requires an interactive Windows console with virtual-terminal and mouse-input support.

## Tabs and actions

- **Distros**: browse installed distributions, launch a shell, stop a distribution, set the default, refresh the list, export a distribution, or unregister it.
- **Online**: refresh the catalog and start installation of a selected distribution.
- **Import**: register a distribution from an installation location and root filesystem archive.
- **Diagnostics**: load WSL status/version details and inspect or stop processes in a running distribution.
- **Lifecycle**: run shutdown, update, or rollback operations with status and command output.
- **Containers**: list `wslc` container IDs; start, stop, or remove a selected container. Removal requires confirmation and does not force-remove a running container. Use `wslc run` to create containers.
- **Help**: view the built-in reference and troubleshooting notes.

## Controls

- `Up` / `Down`: select rows or scroll a list.
- `Left` / `Right`: change tabs; `1` through `7` jump directly to a tab.
- In Containers, `R` refreshes, `S` starts, `X` stops, and `D` removes the selected container.
- `Enter` / `Space`: activate the selected item or action.
- `F7`: open or close full diagnostics.
- `F8`: open Lifecycle.
- `F9`: show or hide the selected distribution's process list in Diagnostics.
- `T`: cycle the color theme; `G`: cycle glyph style.
- Mouse clicks select tabs and distributions; the wheel scrolls lists.
- `Q`: exit.

The selected distribution must be running to inspect its processes. Unregistering a distribution permanently removes it after confirmation.

## Build

From this directory, run `build.ps1` (PowerShell), `build.bat`, or `sh build.sh` (Git Bash). Builds install `wslmgr.exe` into `bin`; use `-NoInstall` or `--no-install` to keep the executable in this directory. Use `-Clean` or `clean` to remove local build artifacts.