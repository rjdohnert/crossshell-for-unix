# machinfo

## What it does

Displays a comprehensive hardware, OS, and security summary for Windows, similar to HP-UX `machinfo`. Collects OS configuration, hardware topology, memory utilization, network adapters, and native Trusted Platform Module (TPM 1.2 / 2.0) state via the Windows TBS API.

## Usage

- `machinfo [options]`

## Options

- `-f`, `--format <table|json|csv>`: select output layout (default `table`)
- `-t`, `--tpm-only`: restrict execution and output strictly to TPM subsystem telemetry
- `-v`, `--verbose`: include detailed network adapter configuration and hardware IDs
- `-n`, `--no-color`: suppress ANSI terminal escape sequences
- `-V`, `--version`: display version information and exit
- `-h`, `--help`: print the manual page and exit

## Output formats

- **table** (default): aligned key/value listing with optional ANSI color. Color is only used on an interactive console; when stdout is redirected to a file or pipe, color is automatically disabled and the console mode is restored on exit.
- **json**: a single JSON object. All string values are escaped; measurements and flags use numeric/boolean types. Includes a `Network` array with one object per adapter.
- **csv**: a header row plus one summary row, then a NIC section with one row per adapter. All values are quoted and escaped.

## Data sources

- Windows registry (`CurrentVersion`, BIOS, CPU) for OS and hardware identity
- `GetNativeSystemInfo` for processor architecture
- `GlobalMemoryStatusEx` for physical/virtual memory
- TPM Base Services (TBS) device-info and context-creation probe for TPM presence/readiness
- `GetAdaptersAddresses` for network adapter enumeration

## Notes

- The TPM context-creation probe treats access denied (`TBS_E_ACCESS_DENIED`) as "present and enabled for a standard user," not full readiness.
- On restricted environments some TPM fields can remain `N/A`.
- Boot time is derived from `GetTickCount64` and may drift across suspend/hibernate.

## Examples

```text
machinfo
machinfo -t
machinfo --format json
machinfo --format json | jq .System.Memory
machinfo -v --format csv
```

## Build

```text
src\machinfo\build.bat       (auto-detects clang / cl / gcc, installs to bin\)
src\machinfo\build.ps1       (same; -Clean, -NoInstall, -Compiler clang|cl|gcc)
```

Links against `tbs`, `iphlpapi`, `advapi32`, and `ws2_32`.

