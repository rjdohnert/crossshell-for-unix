# xls

## What it does

Extended `ls` for Windows NT. Lists directory contents with NTFS-aware telemetry that plain `ls`/`dir` do not surface: per-file cluster allocation, disk slack, extended attribute flags, and volume cluster/serial information.

## Usage

- `xls [options] [path]`

## Options

- `-a`, `--all`: include hidden and system files (hidden by default)
- `-l`, `--long`: full extended listing (timestamps, allocations, attributes, names)
- `-c`, `--clusters`: display allocated cluster count per file (implies `-l`)
- `-R`, `--recursive`: recursively scan and list subdirectories
- `-s`, `--sort <name|ext|size|date>`: sort entries (default `name`); directories always sort first
- `-r`, `--reverse`: reverse the sort order
- `-h`, `--human-readable`: (default) sizes are shown with unit suffixes (B, KB, MB, GB, TB)
- `-n`, `--no-color`: disable ANSI terminal coloring
- `-V`, `--version`: display version and exit
- `-H`, `--help`: display the manual and exit

## Attribute flags

The `Attrib` column uses single-letter NTFS/DOS flags (`.` = unset):

- `R` = Read-Only, `H` = Hidden, `S` = System, `A` = Archive
- `D` = Directory, `C` = Compressed, `E` = Encrypted, `L` = Reparse/Link

## Behavior

- Sizes are always human-readable (KB/MB/GB/TB); there is no raw-bytes mode.
- The `Allocated` column shows the on-disk cluster allocation (size rounded up to the volume cluster size); `Slack` in the summary is allocated minus payload.
- Directory rows show `<DIR>` and do not compute a recursive content size.
- When stdout is redirected to a file or pipe, color is automatically disabled so no ANSI escape sequences leak into captured output; the console mode is restored on exit.
- Volume header shows drive root, label, serial number, filesystem type, and cluster size.

## Examples

```text
xls -l
xls -l -c C:\Windows
xls -l -s size -r
xls -R -a -l
xls -l > listing.txt
```

## Build

```text
src\xls\build.bat            (auto-detects clang / cl / gcc, installs to bin\)
src\xls\build.ps1            (same; -Clean, -NoInstall, -Compiler clang|cl|gcc)
```

## UNIX origin

An original CrossShell utility; not a direct port of a classic Unix command, but modeled on `ls`/`dir` with added Windows filesystem telemetry.
