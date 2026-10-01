# setboot

## What it does

Views and updates UEFI boot parameters in an HP-UX-style `setboot` workflow.

## Usage

- `setboot [-p id] [-a id] [-b on|off] [-t seconds] [-v] [-h]`

## Options

- `-p <id>`: set primary boot target by moving `BootXXXX` to the front of `BootOrder`
- `-a <id>`: set alternate one-time boot target (`BootNext`)
- `-b <on|off>`: enable or disable autoboot delay
- `-t <seconds>`: set timeout value in seconds
- `-v`, `--verbose`: include EFI path and attribute details
- `-h`, `--help`, `/?`: help

## Privileges

- Viewing some UEFI variables may require elevation depending on policy.
- Modifying NVRAM values (`-p`, `-a`, `-b`, `-t`) requires Administrator rights and `SeSystemEnvironmentPrivilege`.

## Notes

- If `BootOrder` cannot be read, the command now reports a warning and Windows error code.
- Works only on UEFI systems.

## Examples

- `setboot`
- `setboot -v`
- `setboot -p Boot0001`
- `setboot -a Boot0002`
- `setboot -t 10`
- `setboot -b off`
