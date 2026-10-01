# zgrep

## What it does
Searches inside `.gz` files using `grep` semantics.

## Options
- Accepts most standard `grep` options and forwards them to `grep`
- `-h`, `--help`: show zgrep wrapper help
- `--version`: show version information

## Notes
- Baseline implementation decompresses gzip inputs to temporary files, then runs `grep`.
- `stdin` mode is not supported in this baseline build.

## UNIX origin
`zgrep` is the traditional Unix helper that enables `grep`-style pattern matching directly on compressed files.
