# machinfo

## What it does

Displays a machine hardware and firmware summary similar to HP-UX `machinfo`, using Windows-native APIs.

## Usage

- `machinfo [options]`

## Options

- `-v`, `--verbose`: include extended processor capability details
- `-q`, `--quiet`: condensed one-screen summary
- `-h`, `--h`, `--help`, `-?`, `/?`: help

## Data sources

- SMBIOS firmware tables (system/BIOS identity)
- Logical processor topology APIs (socket/core/thread/cache)
- TPM Base Services + registry + optional WMI fallback (TPM status/version)
- Windows memory status APIs
- NT kernel version query path

## Notes

- On restricted environments, some TPM fields can remain `N/A` while presence/version are still detected.
- Quiet mode is useful for scripts; verbose mode adds cache and virtualization details.

## Examples

- `machinfo`
- `machinfo -q`
- `machinfo -v`
- `machinfo --h`
