# mvdir

## What it does

Moves or renames a directory. Reimplementation of the AIX `mvdir` command for Windows environments.

## Options

- `-h`, `--help`, `/?`: show help text
- `-v`, `--version`: show version and license information

## Usage

- `mvdir Directory1 Directory2`

## Behavior

- If `Directory2` exists and is a directory, `Directory1` is moved inside it as `Directory2\Directory1`.
- If `Directory2` does not exist, `Directory1` is renamed to `Directory2` provided the parent of `Directory2` exists.
- If source and target reside on different volumes, a recursive copy-and-delete is performed automatically.

## Safety Invariants

- Cannot move `.` or `..` or drive root directories.
- Cannot move a directory into itself or a subdirectory of itself.
- Cannot overwrite an existing file or a pre-existing target directory.

## Exit Status

- `0`: success
- `1`: failure

## AIX origin

A classic AIX system administration command for moving directory trees.
