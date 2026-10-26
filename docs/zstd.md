# zstd

Compresses and decompresses Zstandard data, with file, standard-stream, integrity
test, listing, and benchmark modes. This documentation follows the preserved
original implementation in `src/zstd/zstd.c.orig`.

## Usage

```text
zstd [OPTIONS] [-|INPUT-FILE] [-o OUTPUT-FILE]
```

Compression is the default mode and uses level 3 by default. For a named input,
compression writes `INPUT-FILE.zst`; decompression removes the `.zst` suffix
when present. Existing output files are not overwritten unless `-f` is used.
With no input file, zstd reads standard input and writes to standard output.

```powershell
zstd archive.tar
zstd -d archive.tar.zst
zstd -t archive.tar.zst
zstd -c data.json > data.json.zst
```

## Options

### Modes and compression

- `-z`, `--compress`: compress (the default mode).
- `-d`, `--decompress`: decompress; `--uncompress` is an alias.
- `-t`, `--test`: test compressed-file integrity.
- `-l`, `--list`: display information about compressed files.
- `-b#`: benchmark files at compression level `#`.
- `-#`: set compression level from 1 to 19 (default `3`).
- `--ultra`: allow levels 20 through 22.
- `--fast[=#]`: select ultra-fast compression mode.
- `-T#`, `--threads=#`: set worker-thread count; `0` selects available processors.

### Files and output

- `-o FILE`, `--output=FILE`: choose the output file.
- `-c`, `--stdout`: write output to standard output.
- `-f`, `--force`: overwrite an existing output file.
- `-k`, `--keep`: preserve input files (the default).
- `--rm`: remove input files after successful processing.
- `--`: end option parsing; following arguments are treated as input paths.

### Display and frame options

- `-v`, `--verbose`: show additional status; repeat for more verbosity.
- `-q`, `--quiet`: suppress warnings and normal output.
- `--no-progress`: suppress continuous terminal progress.
- `-C`, `--check`: include a 32-bit XXH64 frame checksum (enabled by default).
- `--no-check`: disable the frame checksum.
- `--content-size`: record the uncompressed size (enabled by default).
- `--no-content-size`: omit the uncompressed size.
- `-h`, `-H`, `--help`: show help; `-V`, `--version`: show version.