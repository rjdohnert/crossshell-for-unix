# dskctl

`dskctl` is a preview-first Windows utility for inspecting and cleaning approved
temporary, cache, log, crash-dump, and Recycle Bin locations.

## Usage

```text
dskctl [OPTIONS] [TARGETS...]
```

With no target selector, `dskctl` previews the low-risk targets. Preview mode
does not modify files.

## Options

| Option | Description |
| --- | --- |
| `-n`, `--dry-run` | Preview only (default). Cannot be combined with `--execute`. |
| `-x`, `--execute` | Clean selected targets after preview and confirmation. Requires an elevated Administrator process. |
| `-s`, `--safe` | Select all low- and medium-risk targets. High-risk targets cannot be explicitly combined with this option. |
| `-a`, `--all` | Select every target, including high-risk targets. |
| `-t`, `--trash` | Move removed items to the Recycle Bin where supported. |
| `-v`, `--verbose` | Show up to five sample paths per target. |
| `-f`, `--force` | Bypass interactive execution confirmations. Use only with a deliberate target selection. |
| `--json` | Emit JSON output. |
| `--csv` | Emit CSV output. |
| `--table` | Emit tab-separated output. |
| `-h`, `--help` | Display help and exit. |

Only one of `--json`, `--csv`, or `--table` may be specified. Structured output
contains target counts and bounded issue details. Combining structured output
with `--execute` requires `--force`; this avoids mixing prompts with
machine-readable output.

## Targets

| Option | Description | Risk |
| --- | --- | --- |
| `--temp` | Current user's LocalAppData temporary directory | Low |
| `--system-temp` | Windows temporary directory | Medium |
| `--update-cache` | Downloaded Windows Update packages | Medium |
| `--error-dumps` | Windows memory dump and Minidump files | High |
| `--logs` | Windows setup and system logs | High |
| `--recycle-bin` | Current user's Recycle Bin | High |
| `--wer` | User crash dumps and Windows Error Reporting files | High |
| `--shaders` | Known GPU and DirectX shader-cache locations | Medium |
| `--delivery-opt` | Windows Delivery Optimization cache | Medium |
| `--inet-cache` | Current user's INetCache directory | Medium |

Target paths are derived from Windows Known Folders and the Windows directory,
not from untrusted environment variables. Cleanup is constrained to these
approved roots. Reparse points (including symbolic links and junctions) are
skipped, and directory traversal has depth and entry limits. Preview and
cleanup reports include encountered skips and errors; output is bounded.

Execution presents a preview, then requires confirmation unless `--force` is
used. The Recycle Bin option changes the removal method where supported; it
does not expand the approved cleanup roots.

## Examples

```text
dskctl --safe
dskctl --temp --logs
dskctl --temp --execute
dskctl --temp --execute --force
dskctl --safe --json
dskctl --temp --csv
dskctl --temp --execute --force --json
```
