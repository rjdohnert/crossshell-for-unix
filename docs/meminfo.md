# meminfo

## Name

`meminfo` - report physical memory, commit charge, kernel pools, and paging-file usage.

## Synopsis

```text
meminfo [QUALIFIERS]
```

## Description

`meminfo` reports Windows physical-memory and virtual-memory statistics in a table by default. It can also emit JSON or CSV for scripts and monitoring tools. The report includes total, used, cached, and available physical memory; commit limit and usage; and, when requested, kernel pool and individual paging-file details.

## Qualifiers

### Display units

- `-h`, `--human` - use human-readable binary units. This is the default.
- `-b`, `--bytes` - show raw byte counts.
- `-k`, `--kibi` - show values in KiB.
- `-m`, `--mebi` - show values in MiB.
- `-g`, `--gibi` - show values in GiB.

### Output formats

- `--table` - emit the standard aligned report.
- `--json` - emit a JSON snapshot with byte counts.
- `--csv` - emit comma-separated records.

### Details and monitoring

- `-d`, `--devices` - include per-pagefile totals, usage, free capacity, and percentage used.
- `-w`, `--wide` - include kernel paged/non-paged pools and pagefile details.
- `-s`, `--seconds N` - repeat the report every `N` seconds.
- `-c`, `--count N` - stop after `N` reports when repeating.

### Information

- `-?`, `--help` - display usage and qualifier information.
- `-v`, `--version` - display the utility version.

## Examples

```text
meminfo
meminfo --bytes --json
meminfo --wide --mebi
meminfo --csv --seconds 5 --count 12
```

## Exit status

- `0` - report completed successfully.

## Platform notes

`meminfo` uses Windows memory-status, performance-information, and paging-file APIs. Values are binary units: KiB is 1024 bytes, MiB is 1024 KiB, and GiB is 1024 MiB.
