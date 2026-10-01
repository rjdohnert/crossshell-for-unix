# swlist

## What it does

Lists installed software in an HP-UX-like `swlist` style by reading Windows uninstall registry hives.

## Usage

- `swlist [-l level] [-a attribute] [-v] [pattern ...]`

## Options

- `-l <level>`: listing level (`product` default, `bundle` alias of product, `vendor`, `all`)
- `-a <attribute>`: output attribute view (`title`, `revision`, `vendor`, `date`, `location`, `arch`, `all`)
- `-v`, `-f`, `--verbose`: full multi-column listing
- `-h`, `--help`, `/?`: help

## Notes

- Reads from:
- `HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall` (x64 + x86)
- `HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall`
- Level `all` includes system components.
- Pattern operands are case-insensitive and match product name and vendor text.

## Examples

- `swlist`
- `swlist -v`
- `swlist -l vendor`
- `swlist -a revision Microsoft`
- `swlist Python`
