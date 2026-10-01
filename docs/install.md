# install

## What it does
Copies or moves files and directory trees with filtering, retry handling, mirroring, dry-run output, and optional worker threads.

## Usage
`install [options] <source> <destination> [file [file ...]]`

## Copy options
- `-s`, `--subdirs`: copy subdirectories except empty ones
- `-e`, `--all-subdirs`: copy subdirectories including empty ones
- `-m`, `--mirror`: copy all subdirectories and purge extra destination entries
- `--purge`: remove destination entries absent from the source
- `-l n`, `--level=n`: limit directory traversal depth
- `--move-files`: delete source files after successful copies
- `--move-all`: delete the source tree after successful copying
- `--create-zero`: create the tree with zero-length destination files

## Filtering and reliability
- `--exclude-changed`, `--exclude-newer`, `--exclude-older`: filter files by source/destination state
- `--exclude-files=pattern`, `--exclude-dirs=pattern`: exclude matching names
- `--max-size=bytes`, `--min-size=bytes`: filter by file size
- `-r n`, `--retries=n`: set retries after a failed copy; the default is 3
- `-w n`, `--wait=n`: set seconds between retries; the default is 5
- `-j n`, `-t n`, `--threads=n`: set worker count; the default is 1

## Output options
- `-n`, `-L`, `--dry-run`: report planned work without changing files
- `-v`, `--verbose`: include skipped and identical files
- `--no-progress`, `--no-file-list`, `--no-dir-list`: suppress selected progress details
- `--no-header`, `--no-summary`: suppress the job header or final summary
- `-h`, `--help`: show help text
- `-V`, `--version`: show version information

## Examples
```text
install source.txt destination.txt
install --all-subdirs --threads=4 source destination
install --mirror --dry-run source destination
```

## Exit status
The result is a bitmap: `1` indicates copied files, `2` extra destination entries, `4` mismatches, `8` failed copies, and `16` a fatal parameter or source error. A result of `0` means no errors and no files copied.