# mktemp

## What it does
Safely creates a unique temporary file or directory and prints its path. Names are generated with the Windows system CSPRNG and protected file security defaults.

## Usage
```text
mktemp [OPTION]... [TEMPLATE]
```

`TEMPLATE` must contain at least three consecutive `X` characters in its final path component. When omitted, the default template is `tmp.XXXXXXXXXX` and the system temporary directory is used.

## Options
- `-d`, `--directory`: create a directory instead of a file.
- `-u`, `--dry-run`: generate and print a name without creating it; this is unsafe for security-sensitive use.
- `-q`, `--quiet`: suppress diagnostics for file or directory creation failures.
- `--suffix=SUFFIX`: append `SUFFIX`; it must not contain a path separator. It is implied when the template does not end in `X`.
- `-p DIR`, `--tmpdir[=DIR]`: resolve a relative template under `DIR`; without `DIR`, use the configured temporary-directory environment variables or Windows fallback.
- `-t`: treat the template as one file-name component relative to the temporary directory.
- `-h`, `--help`: display help text.
- `-V`, `--version`: display version information.

Temporary directory variables are checked in this order: `TMPDIR`, `TMP`, `TEMP`, then the Windows `GetTempPathW` fallback.

## Examples
```text
mktemp
mktemp -d build.XXXXXXXX
mktemp -p C:\Temp session.XXXXXX
mktemp --suffix=.log log.XXXXXX
mktemp -u -t cache.XXXXXXXXXX
```

## Exit status
Returns `0` when creation succeeds or a dry-run name is generated. Returns `1` when validation, random generation, path resolution, or creation fails.

## Windows notes
The command uses Windows wide-character APIs, BCrypt system-preferred random generation, protected security descriptors, and extended kernel paths for long temporary paths.

## UNIX origin
A Windows-native implementation modeled on the POSIX `mktemp` utility.
