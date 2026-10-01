# lsdev

## What it does
Lists local hardware devices with AIX-inspired status and class output.

## Usage
lsdev [options]

## Options
- -a: include non-present devices (default shows present devices only).
- -C CLASS: filter by class name (case-insensitive substring).
- -h, --help: display help and exit.
- -v, --version: display version and exit.

## Exit Codes
- 0: success
- 1: runtime error or no matching devices
- 2: invalid command syntax

## Examples
```powershell
lsdev
lsdev -C Net
lsdev -a -C Disk
```

## Windows Mapping Notes
- Enumerates device nodes through SetupAPI.
- Status is derived from CM_Get_DevNode_Status and mapped to AIX-style words.
- Default table output uses clean fixed-width columns: Status, Class, Device, and InstanceId.
