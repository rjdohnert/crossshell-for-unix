# groupmod

`groupmod` renames local Windows groups and updates their descriptions through
the Windows NetAPI. It does not modify Active Directory groups, assign Unix
GIDs, or set Windows RIDs.

## Usage

```text
groupmod [OPTIONS] GROUP
groupmod [OPTIONS] -
groupmod [OPTIONS] < modifications.csv
```

| Option | Description |
| --- | --- |
| `-n`, `--new-name NEW_GROUP` | Rename the group. |
| `-c`, `--comment COMMENT` | Set or clear the group's description. |
| `-f`, `--force` | Succeed if the source group is absent. Does not suppress a name collision or bypass permissions/protection. |
| `-q`, `--quiet` | Suppress progress and success messages; errors remain visible. |
| `-v`, `--verbose` | Display progress and per-group results. |
| `-h`, `--help`, `/?`, `-?` | Display help. |
| `-V`, `--version` | Display version information. |
| `--` | Treat the remaining operand as a group name, even if it begins with `-`. |
| `-` | Explicitly read UTF-8 modification records from standard input. |

At least one of `-n` or `-c` is required. If no group operand is given and
standard input is redirected, modification records are read automatically.
Use `--new-name=VALUE` or `--comment=VALUE` when an option value begins with
`-`.
Records contain `GROUP`, optionally followed by comma- or tab-delimited
`NEW_GROUP` and `COMMENT` fields. Quoted CSV fields support doubled quotes.
One-field records use command-line `-n`/`-c` defaults. An empty new-name field
means do not rename; an empty comment field clears the description. Blank lines
and lines whose first non-whitespace character is `#` are ignored. A UTF-8 BOM
is accepted. Records are limited to 64 KiB; malformed UTF-8, NULs, malformed
quoting, and control characters in group names are rejected.

When both the new name and comment are supplied, they are sent in one Windows
group update. `--force` ignores only a missing source group; it never reports a
rename collision as success.

## Exit status

- `0`: requested changes succeeded, or the source group was absent with `-f`.
- `2`: invalid command-line arguments, input records, or group-name characters.
- `4`: other Windows account-management or input/output failure.
- `6`: source group does not exist and `-f` was not specified.
- `8`: Windows protects the built-in or special group.
- `9`: the requested new group name is already in use.
- `10`: access denied; an elevated terminal may be required.

Input batches continue after individual failures and return the first nonzero
status.

## Examples

```powershell
groupmod -n senior_engineers developers
groupmod -c "Core Engineering Team" devops
groupmod -n secops -c "Security Operations" security
Get-Content .\modifications.csv | .\groupmod.exe -v
```

## Build

Build on Windows with `src\groupmod\build.ps1`. The normal build copies
`groupmod.exe` into `bin\`, where the installer collects it. Modifying local
groups usually requires an elevated terminal.
