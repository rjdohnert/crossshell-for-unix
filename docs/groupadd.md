# groupadd

`groupadd` creates local Windows groups through the Windows NetAPI. It does not
create Active Directory domain groups and cannot assign a Unix GID or Windows
RID.

## Usage

```text
groupadd [OPTIONS] GROUP
groupadd [OPTIONS] -
groupadd [OPTIONS] < groups.csv
```

| Option | Description |
| --- | --- |
| `-c`, `--comment COMMENT` | Set the group's description. For input records, use as the default comment. |
| `-f`, `--force` | Succeed if the group already exists; does not modify it. |
| `-r`, `--system` | Compatibility option; Windows local groups have no system designation. |
| `-q`, `--quiet` | Suppress progress and success messages; errors remain visible. |
| `-v`, `--verbose` | Display progress and per-group results. |
| `-h`, `--help`, `/?`, `-?` | Display help. |
| `-V`, `--version` | Display version information. |
| `-` | Explicitly read UTF-8 records from standard input. |

If no group operand is given and standard input is redirected, input records
are read automatically. A record contains a group name, optionally followed by
a comma or tab and a description. Quoted CSV fields support doubled quotes.
Blank lines and lines whose first non-whitespace character is `#` are ignored.
Input records are limited to 64 KiB; invalid UTF-8 and embedded NUL characters
are rejected. Empty command-line operands and control characters in group
names are also rejected.

When `--system` is specified, the command warns that the option has no effect
on Windows local groups. The group is created as a regular local group.

## Exit status

- `0`: all requested operations succeeded, including an existing group with
  `-f`.
- `2`: invalid command-line arguments, input records, or group-name characters.
- `4`: Windows account-management, standard-input, or output failure.
- `9`: group already exists and `-f` was not specified.

Input batches continue after individual record failures and return the first
nonzero status.

## Examples

```powershell
groupadd developers
groupadd -c "Core Engineering Team" engineers
groupadd -f developers
Get-Content .\groups.csv | .\groupadd.exe -v
```

## Build

Build on Windows with `src\groupadd\build.ps1`. The normal build copies
`groupadd.exe` into `bin\`, where the installer collects it. Group creation
usually requires an elevated terminal.
