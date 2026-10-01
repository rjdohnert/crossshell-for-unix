# lspci

## What it does
Lists PCI devices attached to the system, including vendor, device, class, and hardware path information from the Windows device tree.

## Supported options
- `--table`: print tabular output (default)
- `--csv`: print CSV output
- `--json`: print JSON output
- `-q`, `--quiet`: compact output mode
- `-`: read filters from standard input
- `-h`, `--help`, `/?`: display help text

## Notes
- The implementation enumerates present PCI devices through SetupAPI and reports identifying metadata.
- Filters can match device names, vendor strings, and hardware IDs.
- Output is geared toward quick hardware inventory and troubleshooting.

## Examples

```text
lspci
lspci --json
lspci --csv
lspci NVIDIA
lspci "PCI\VEN_10DE"
```

## UNIX origin
A Unix/Linux hardware-reporting utility rather than a single classic Unix release.
