# lscfg

## What it does
Displays installed hardware resources and Vital Product Data (VPD) in an AIX-style inventory format.

On Windows, it gathers data through WMI (BIOS/system board, CPU, memory modules, disks, NICs, and GPUs).

## Usage
- `lscfg [OPTIONS]`

## Options
- `-v`, `--verbose`: include detailed VPD key/value output
- `-s`, `--summary`: compact summary mode
- `-l`, `--line <class|name>`: filter by class or resource name
- `--version`: show version
- `-h`, `--help`, `/?`: show help

## Supported Filter Classes (`-l`)
- `sys`: system board/BIOS/planar summary
- `cpu`: processor resources
- `mem`: memory modules
- `disk`: storage devices
- `net`: network adapters
- `gpu`: graphics adapters

You can also filter by specific resource names like `proc0`, `mem0`, `hdisk0`, `ent0`, `gpu0`.

## Output Notes
- Prints an AIX-inspired "INSTALLED RESOURCE LIST" table.
- In verbose mode, includes VPD attributes such as serial numbers, part numbers, speeds, and firmware/driver versions.
- Virtual NICs are skipped in adapter listing.

## Examples
- `lscfg`
- `lscfg -v`
- `lscfg -s -l cpu`
- `lscfg -v -l mem`
- `lscfg -v -l hdisk0`

## Exit Status
- `0`: success (including no matching resources for a filter)
- `1`: WMI initialization failure

## UNIX origin
`lscfg` mirrors IBM AIX `lscfg` output style, adapted for Windows data sources.
