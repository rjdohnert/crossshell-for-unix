# ioscan

## What it does

Scans and lists hardware devices in an HP-UX-like format using Windows SetupAPI and device manager data.

## Usage

- `ioscan [qualifiers] [--json|--csv|--table] [--pipe command]`

## HP-UX qualifiers

- `-A`: show alias paths; requires `-N`
- `-a`: show processor socket, core, and thread fields
- `-B`: list pending deferred bindings; always empty on Windows
- `-C <class>`: filter by device class (for example `disk`, `net`, `display`, `processor`)
- `-D`: HP-UX deferred driver binding; recognized but unsupported on Windows
- `-d <driver>`: filter by Windows driver service
- `-e`: include the Windows device-instance path
- `-f`: full view with complete paths
- `-F`: compact colon-delimited output
- `-H <hw_path>`: filter by a hardware-path or device-instance-path prefix
- `-I <instance>`: filter by the instance number within a device class
- `-k`: read the kernel device tree, including non-present phantom nodes
- `-l`: list locally connected devices; all enumerated Windows devices are local
- `-m <keyword>`: display `lun`, `dsf`, or `hw_path` mappings using Windows paths and instance IDs
- `-M <driver>`: HP-UX forced driver binding; recognized but unsupported on Windows
- `-n`: list Windows device-instance identifiers as the device-file equivalent
- `-N`: use agile paths based on Windows device-instance identifiers
- `-P <property>`: display an agile-view property; requires `-N`
- `-R`: HP-UX deferred-binding removal; recognized but unsupported on Windows
- `-s`: list stale, non-present device nodes
- `-t`: display scan time; cannot be combined with other options
- `-u`: list usable devices that are claimed and have a loaded driver
- `-U`: list unclaimed nodes whose hardware type is `INTERFACE`
- `-h`, `--help`, `/?`: show help

## Extended output options

- `--summary`: display counts grouped by device class
- `--json`: emit JSON records
- `--csv`: emit CSV records
- `--table`: emit tab-delimited records
- `--pipe <command>`: write formatted records to another command

Valid `-P` properties include `bus_type`, `cdio`, `is_block`, `is_char`, `is_pseudo`, `b_major`, `c_major`, `minor`, `class`, `driver`, `hw_path`, `id_bytes`, `instance`, `module_path`, `module_name`, `sw_state`, `hw_type`, `description`, `card_instance`, `is_remote`, `health`, `error_recovery`, `is_inst_replaceable`, `wwid`, `uniq_name`, `alias_path`, `physical_location`, and `ms_scan_time`. Properties without a Windows equivalent are reported as `N/A`.

## Notes

- HP-UX class names are mapped to Windows device classes where possible.
- Output state values are normalized to HP-UX-like labels (such as `CLAIMED`, `UNCONFIGURED`, and `DISABLED`).
- Windows SetupAPI does not expose HP-UX LUN paths, device special files, kernel module binding, major/minor numbers, or CDIO metadata. The command uses device-instance IDs where possible and reports unavailable properties as `N/A`.

## Examples

- `ioscan`
- `ioscan -f`
- `ioscan -F -C disk`
- `ioscan -N -P health -C disk`
- `ioscan -d storahci`
- `ioscan -H "PCI"`
- `ioscan -s`
- `ioscan -u`
- `ioscan -C disk`
