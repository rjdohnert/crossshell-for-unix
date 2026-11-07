# groupdel

`groupdel` deletes local Windows groups through the Windows NetAPI. It does
not delete Active Directory groups or modify domain controllers.

## Usage

```text
groupdel [OPTIONS] GROUP...
groupdel [OPTIONS] -
groupdel [OPTIONS] < groups.txt
```

| Option | Description |
| --- | --- |
| `-f`, `--force` | Succeed when a requested group does not exist. Does not bypass permissions or built-in group protection. |
| `-q`, `--quiet` | Suppress progress and success messages; errors remain visible. |
| `-v`, `--verbose` | Display progress and per-group results. |
| `-h`, `--help`, `/?`, `-?` | Display help. |
| `-V`, `--version` | Display version information. |
| `--` | Treat remaining arguments as group names, including names beginning with `-`. |
| `-` | Explicitly read UTF-8 records from standard input. |

Multiple group names may be supplied. If no group operand is given and standard
input is redirected, records are read automatically. A record contains one
group name, optionally followed by a comma or tab and a description. Quoted CSV
fields support doubled quotes. Blank lines and lines whose first non-whitespace
character is `#` are ignored. A UTF-8 BOM is accepted. Records are limited to
64 KiB; invalid UTF-8, NULs, malformed quoting, and control characters in
group names are rejected.

## Exit status

- `0`: all requested deletions succeeded, including missing groups with `-f`.
- `2`: invalid command-line arguments, input records, or group-name characters.
- `4`: other Windows account-management or input/output failure.
- `6`: group does not exist and `-f` was not specified.
- `8`: Windows protects the built-in or special group.
- `10`: access denied; an elevated terminal may be required.

Input batches continue after individual failures and return the first nonzero
status.

## Examples

```powershell
groupdel developers
groupdel qa_team old_group
groupdel -f old_group
Get-Content .\groups.txt | .\groupdel.exe -v
```

## Build

Build on Windows with `src\groupdel\build.ps1`. The normal build copies
`groupdel.exe` into `bin\`, where the installer collects it. Deleting local
groups usually requires an elevated terminal.
