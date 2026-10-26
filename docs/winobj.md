# winobj

## What it does

Browses Windows NT Object Manager directories and enumerates named pipes. Results can be displayed as a table or emitted in formats suited to scripts and pipelines.

## Usage

```text
winobj [OBJECT_SWITCH] [OPTIONS]
```

## Object switches

- `-pipes`, `--pipes`: enumerate named pipes.
- `-mutexes`, `--mutexes`: enumerate Mutant objects in `\\BaseNamedObjects`.
- `-events`, `--events`: enumerate Event objects in `\\BaseNamedObjects`.
- `-sections`, `--sections`: enumerate Section objects in `\\BaseNamedObjects`.
- `-dir PATH`: enumerate all object types in the specified NT directory. The default path is `\\BaseNamedObjects`.

With no object switch, all object types in `\\BaseNamedObjects` are listed.

## Options

- `-f FORMAT`, `--format FORMAT`: choose `table`, `json`, `csv`, or `raw` output. The default is `table` for terminal output and `raw` (tab-separated) when redirected or piped.
- `-S ID`, `--session ID`: inspect `\\Sessions\<ID>\BaseNamedObjects` instead of the default object directory. This option does not change named-pipe enumeration.
- `-h`, `--help`, `/?`: display usage information.

## Output

Each result contains its object type, name, and NT namespace path. JSON uses `type`, `name`, and `path`; CSV uses `Type`, `Name`, and `Path`; raw output is tab-separated in that same field order.

## Examples

```powershell
winobj
winobj -pipes
winobj -mutexes --format json
winobj -events | grep -i Global
winobj -dir "\RPC Control" --format csv
winobj -mutexes -S 1
```

## Exit status

- `0`: output completed or help was displayed.
- `1`: an unknown option or format was provided, or the NT Object Manager APIs could not be loaded.

## Notes

NT directory enumeration uses the native APIs exported by `ntdll.dll`; directories the current process cannot open may produce no results. Named pipes are enumerated through the Windows pipe namespace.

This tool was inspired by WinObj and the work of Mark Russinovich and the Sysinternals team. Sysinternals is now part of Microsoft.