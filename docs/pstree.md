# pstree

## What it does

Displays running processes as a tree, illustrating parent-child relationships between active processes on Windows.

## Usage

- `pstree [OPTIONS] [PID]`

## Options

- `-p`, `--show-pids`: show Process IDs alongside process names in the format `name.exe(PID)`
- `-a`, `--arguments`, `--full-path`: display the full executable path instead of just the image name
- `-n`, `--numeric-sort`: sort child processes by PID numerically instead of alphabetically by name
- `-s`, `--show-parents`: trace ancestry back to the system root for a specific target PID
- `-A`, `--ascii`: use ASCII branch characters (`|--`, `` `-- ``)
- `-U`, `--unicode`: use Unicode box-drawing characters (`├──`, `└──`, `│`)
- `-c`, `--color`: enable ANSI colorized output (root nodes bold red, PIDs cyan)
- `-h`, `--help`: display the help manual and exit
- `-v`, `--version`: display version and build details

## Exit status

- `0`: successful completion
- `1`: invalid arguments or syntax error
- `2`: process snapshot failure
- `3`: specified target PID not found

## Notes

- Without a PID argument the full system process tree is shown rooted at process 0.
- `-s/--show-parents` requires a PID to be specified.
- Full executable paths for elevated or system processes require Administrator privileges.
- Recycled-PID loop detection prevents infinite cycles in the tree.

## Examples

- `pstree -p`
- `pstree -p -c 4120`
- `pstree -p -s 4120`
- `pstree -A -n`
- `pstree -a -p`
- `pstree --help`
- `pstree --version`

## UNIX origin

A Unix/Linux utility present in procps and similar packages for visualizing process hierarchies.
