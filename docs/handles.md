# handles

`handles` reports per-process handle counts for running Windows processes.
It enumerates processes with the Tool Help API and queries accessible
processes with `GetProcessHandleCount`.

## Usage

```text
handles [OPTIONS]
```

| Option | Description |
| --- | --- |
| `-f`, `--format`, `--output FORMAT` | Choose `table`, `csv`, `json`, `tsv`, or `pipe`. |
| `-s`, `--sort COLUMN` | Sort by `handles`, `pid`, or `name`. |
| `--asc`, `--desc` | Select ascending or descending order; default is descending. |
| `-n`, `--name NAME` | Filter by a case-insensitive process-name substring. |
| `-p`, `--pid PID` | Filter by an exact process ID, including PID 0. |
| `-t`, `--top COUNT` | Limit results after filtering and sorting; zero means unlimited. |
| `-j`, `--json` | Select JSON output. |
| `--csv`, `--tsv`, `--table` | Select the corresponding output format. |
| `--pipe COMMAND` | Send formatted output to a command; report unsuccessful delivery. |
| `-h`, `--help`, `/?`, `-?` | Display help. |
| `-V`, `--version` | Display version information. |

## Output

The table, CSV, TSV, pipe-delimited, and JSON formats report the PID, process
name, handle count, and query status. A count is unavailable when a process
exits during collection or Windows denies access. Structured formats expose
this status explicitly; JSON uses `null` for an unavailable count.

Sorting by handle count places accessible counts in the requested order and
unavailable counts last. Equal values are ordered by PID.

## Exit status

- `0`: collection and output completed. Individual inaccessible processes are
  reported with their status and do not fail the whole query.
- `1`: process enumeration, output, or pipe delivery failed.
- `2`: command-line arguments are invalid.

## Notes

The collector uses documented Windows process APIs and requests only
`PROCESS_QUERY_LIMITED_INFORMATION`. Some protected or otherwise inaccessible
processes cannot provide a count and remain in the output with an appropriate
status.

Build on Windows with `src\handles\build.ps1`. The normal build copies
`handles.exe` to `bin\`, where the installer harvests it.
