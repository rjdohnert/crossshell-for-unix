# uncompress

## What it does
Decompresses files compressed using the classic Unix LZW (.Z) format. Fully compatible with archives created by `compress`, `ncompress`, and other POSIX-compliant LZW tools.

## Options
- `-c`, `--stdout`: write decompressed data to standard output; leave files unchanged
- `-f`, `--force`: force overwrite of destination files without prompting
- `-k`, `--keep`: retain source .Z archive after successful decompression
- `-q`, `--quiet`: suppress all non-critical warnings
- `-v`, `--verbose`: print compression ratios and file replacements
- `-V`, `--version`: show version information
- `-h`, `--help`: show help text
- `--`: terminate option scanning

## Features
- Supports classic UNIX compress 4.x files (9-16 bits, variable length codes)
- Native Windows wildcard expansion (`*`, `?`)
- Atomic file replacement with temp-file + `ReplaceFileW`
- Timestamp preservation from source archive
- Hardened against circular prefix references and buffer overflows (CVE-2006-1168)
- Long path support via `\\?\` prefixing
- Graceful Ctrl-C cleanup of temporary files

## UNIX origin
A traditional Unix decompression utility from the BSD and System V era, companion to `compress`.
