# realpath

## What it does
Resolves one or more file names to absolute, normalized Windows paths and prints one result per input.

## Usage
```text
realpath [OPTION]... FILE...
```

## Options
- `-e`, `--canonicalize-existing`: require all path components to exist (default).
- `-m`, `--canonicalize-missing`: allow missing path components.
- `-s`, `--strip`, `--no-symlinks`: normalize `.` and `..` without resolving symlinks.
- `-q`, `--quiet`: suppress most error messages.
- `--relative-to=DIR`: print each result relative to `DIR`.
- `--relative-base=DIR`: print a relative result when the result is within `DIR`; otherwise print the absolute result.
- `-z`, `--zero`: terminate results with NUL instead of newline.
- `--help`: display help text.
- `--version`: display version information.

## Examples
```text
realpath .
realpath -m non\existing\sub\dir\file.txt
realpath --relative-to=C:\Projects C:\Projects\App\main.cpp
realpath -s symlink_to_dir
```

## Exit status
Returns `0` when all input paths are resolved. Returns `1` when an operand is missing, an option argument is missing, or path resolution fails.

## Windows notes
The implementation uses `std::filesystem`, emits preferred Windows separators, and removes `\\?\` and `\\?\UNC\` prefixes from resolved output.

## UNIX origin
A Windows-native implementation of the POSIX `realpath` pathname utility.
