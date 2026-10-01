# truncate

## What it does
Shrinks or extends files to an exact, relative, rounded, or reference-file size. Extended regions read as zero bytes and may be sparse on NTFS.

## Usage
```text
truncate [-c] -s [+|-|%|/]size[b|k|m|g|t|p|e] file ...
truncate [-c] -r reference-file file ...
```

## Options
- `-c`: do not create missing target files
- `-r file`: use the size of a reference file
- `-s size`: set an exact size
- `-s +size`, `-s -size`: increase or decrease the current size
- `-s %size`, `-s /size`: round down or up to a multiple of the size
- `-h`, `--help`: show help text
- `-V`, `--version`: show version information

Size suffixes are case-insensitive: `b` means 512-byte blocks, while `k`, `m`, `g`, `t`, `p`, and `e` use powers of 1024.

## Examples
```text
truncate -s 10M disk.img
truncate -s +512K data.bin
truncate -r template.bin output.bin
truncate -c -s 0 existing.log
```

## UNIX origin
Compatible with the common Unix `truncate` utility size syntax.