# gzip

## What it does
Compresses files into `.gz` format.

## Options
- `-d`, `--decompress`: decompress instead of compress
- `-k`, `--keep`: keep input files after successful processing
- `-f`, `--force`: overwrite output file if it exists
- `-h`, `--help`: show help text
- `--version`: show version information

## Notes
- Baseline implementation currently supports local file mode only.
- `stdin/stdout` streaming mode is not implemented in this baseline.

## UNIX origin
`gzip` is the standard Unix file compressor using the DEFLATE algorithm and `.gz` container.
