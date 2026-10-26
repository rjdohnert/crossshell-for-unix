# ls

## What it does
Lists files and directories, expanding `*` and `?` patterns and reporting Windows-native attributes and ACLs.

## Supported options
- `-a`: include hidden and dot entries such as `.` and `..`
- `-A`: include almost-all entries except `.` and `..`
- `-c`: use creation time for display or time sorting
- `-C`: display entries in vertical columns
- `-d`: display a directory itself instead of its contents
- `-e`: long format plus per-trustee ACL details
- `-f`: include hidden entries and disable sorting
- `-b`: escape shell-special and non-printing characters
- `-F`: append type indicators (`/` for directories, `*` for executables, `@` for symlinks)
- `-H`: follow command-line symbolic links during recursive traversal
- `-L`: follow symbolic links during recursive traversal, with cycle detection
- `-P`: do not follow symbolic links (default)
- `-h`: human-readable sizes in long format, using base-1024 units
- `-i`: display the Windows file ID before each entry (`?` if unavailable)
- `-k`: report allocated blocks in 1KB units when used with `-s`
- `-l`: long format with native Windows attributes, owner, size, and timestamp
- `-m`: comma-separated listing
- `-N`: output filenames literally without quoting
- `-Q`: quote filenames with C-style quoting and escapes
- `-p`: append `/` to directory names
- `-r`: reverse sort order
- `-R`: list directories recursively (reparse-point directories are not followed)
- `-s`: display allocation size in blocks
- `-S`: sort by file size
- `-t`: sort by modification time
- `-v`: natural/version sort (`item2` before `item10`)
- `-X`: sort by file extension
- `-u`: use access time for display or time sorting
- `-1`: force one entry per line
- `-x`: display entries in horizontal rows
- `--acl`: list allow/deny `rwx` rights per DACL trustee
- `--group-directories-first`, `--group-directories-last`: group directories at either end
- `--color`: force colored output
- `--no-color`: disable colored output
- `--quoting-style=STYLE`: select `literal`, `escape`, `shell`, or `c`
- `--width=N`: set width used by column layouts
- `--help`, `-?`: show help

## Notes
- The `-l` mask reports Windows file attributes (`d`, `r`, `a`, `h`, `s`, `c`, `e`, `l`) and `+` when a DACL is present; it does not emulate Unix permissions.
- `-s` uses the allocation size reported by Windows; `-i` shows the volume-local Windows file index, not a POSIX inode.
- Default output uses columns in a console and one entry per line when redirected.
- Column widths account for full-width and combining Unicode characters. `--width=N` supports deterministic or narrow layouts.
- Redirected text output uses UTF-8. Windows filenames cannot contain newlines or control characters; use `-b` or shell quoting for valid shell metacharacters.
- Terminal colors: directories blue, source files orange, audio/video media purple, images pink, archives red, and Windows installer packages cyan. Directories and installers take precedence over extension categories; images take precedence over media and source extensions.
- Colors honor `NO_COLOR` and accept safe `LS_COLORS` directory (`di=CODE`) and extension (`*.ext=CODE`) rules with numeric ANSI SGR codes.
- Color is enabled automatically for console output; use `--color` or `--no-color` to override.

## Examples

```text
ls -l
ls -laF C:\Windows\System32
ls -ltr C:\Projects
ls -1S E:\Backups
ls --acl C:\Windows\System32\cmd.exe
```

## UNIX origin
A core Unix directory-listing utility from early BSD and System V releases.
