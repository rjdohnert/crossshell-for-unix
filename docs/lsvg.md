# lsvg

## What it does

Reports volume groups in an AIX-style layout for Windows-backed storage. It presents the Windows OS disk layout as a simplified `lsvg`-like view with volume groups, physical volumes, and logical volumes modeled after the AIX command output.

## Usage

- `lsvg.exe [options] [vg_name]`

## Options

- `-o`: list active volume groups only
- `-l <vg>`: show logical volume details for the named volume group
- `-p <vg>`: show physical volume details for the named volume group
- `-a`: show comprehensive details for all volume groups
- `-help`, `/ ?`, `--help`: show help text

## Notes

- This is a Windows-native approximation of the AIX `lsvg` command rather than a strict LVM implementation.
- The output is intended to resemble the AIX-style summary with volume-group name, state, total/free physical partitions, logical volumes, and basic storage metadata.
- The tool uses Windows disk APIs and logical-drive enumeration to build an emulated `rootvg`/`datavg` model on top of the host storage state.
- Some values are modeled rather than extracted from a true AIX LVM database, so they should be treated as compatibility output rather than authoritative storage metadata.

## Examples

- `lsvg.exe`
- `lsvg.exe -o`
- `lsvg.exe rootvg`
- `lsvg.exe -l rootvg`
- `lsvg.exe -p rootvg`
- `lsvg.exe -a`

## UNIX origin

The original `lsvg` command comes from IBM AIX and is used to report volume groups and the logical/physical volumes associated with them.
