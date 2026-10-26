# machinfo

## What it does

Displays a machine hardware and firmware summary similar to HP-UX `machinfo`, using Windows-native APIs.

## Usage

- `machinfo [options]`

## Options

- `-a`, `--all`: show the complete machine summary
- `-b`, `--bios`: machine identity and BIOS/firmware details
- `-c`, `--cpu`: processor topology and capabilities
- `-m`, `--memory`: physical memory and commit information
- `-t`, `--tpm`: TPM status and version
- `-v`, `--verbose`, `--extended`, `-x`: include cache, virtualization-firmware, and AVX2 details
- `-q`, `--quiet`, `--brief`, `-s`, `--summary`: condensed summary
- `-f`, `--format`, `--output`, `-o` followed by `table`, `csv`, or `json`
- `--format=FORMAT`, `--output=FORMAT`, `--table`, `--csv`, `--json`: select output format
- `--tpm-wmi`: opt into WMI enrichment for TPM metadata
- Positional section filters: `bios`, `firmware`, `machine`, `cpu`, `processor`, `memory`, `ram`, `tpm`, `security`, `os`, `system`
- `-h`, `--help`, `-?`, `/?`, `/help`: help
- `-V`, `--version`: version

## Data sources

- SMBIOS firmware tables (system/BIOS identity)
- Logical processor topology APIs (socket/core/thread/cache)
- TPM Base Services + registry + optional WMI fallback (TPM status/version)
- Windows memory status APIs
- NT kernel version query path

## Notes

- On restricted environments, some TPM fields can remain `N/A`; readiness requires a successful TBS device-info query.
- Quiet mode is useful for scripts; verbose mode adds cache, virtualization-firmware, and AVX2 details.
- Architecture comes from the native Windows system info, and logical processor count includes active processor groups.
- JSON uses numeric/boolean types for measurements and flags. CSV uses the same selected fields as JSON and quotes every value.
- The TPM device-open probe treats access denied as evidence of presence only, not readiness.
- Run `src/machinfo/test.ps1` to build and exercise serializer, option-parser, malformed-SMBIOS, structured-output, and section-filter regressions.

## Examples

- `machinfo`
- `machinfo -q`
- `machinfo -v`
- `machinfo --h`
