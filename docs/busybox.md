# busybox

`busybox` is the single-binary command front end used throughout this repo. It provides a shell plus a growing set of lightweight Unix-style applets for scripting, automation, file handling, and inspection.

## Quick Start

Show the top-level help:

```powershell
busybox --help
```

Start the shell interactively:

```powershell
busybox sh
```

Run a short script inline:

```powershell
busybox sh -c "echo hello && seq 1 3"
```

Run the built-in regression suite:

```powershell
busybox --self-test
```

The suite reports each case and ends with `SELF-TEST RESULT: PASS` when the
applet dispatcher, shell state handling, scripts, nested execution, resource
limits, and job timeouts are working correctly.

## What It Does

- Provides a POSIX-style shell entry point (`sh`, `bash`, `ksh`).
- Bundles small helper tools used by shell scripts and automation.
- Keeps the command set easy to discover with `busybox help <applet>`.

## Common Applet Groups

### Shell and scripting

- `sh`, `bash`, `ksh`
- `seq`, `touch`, `date`, `mktemp`, `kill`
- `clear`, `cls`, `whoami`, `uname`

The shell supports interactive use, `-c` command strings, and script files:

```powershell
busybox sh
busybox sh -c "echo one; echo two"
busybox sh build.sh release x64
busybox -- sh build.sh release x64
```

Inside a script, `$0` is the script name, `$1` and later variables contain the
supplied arguments, and `$#` contains the argument count.

### Directory switching

`cd` changes the working directory for later commands in the same shell,
`-c` string, or script. Windows and slash-style paths are accepted:

```powershell
busybox sh -c 'cd src; pwd; ls -1'
busybox sh -c 'cd project\src && pwd'
busybox sh -c 'cd /project/src && grep -n error logs/*.txt'
busybox sh -c 'cd C:/work/project && sh build.sh'
```

Directory changes made inside a subshell, command substitution, or pipeline
stage do not change the parent shell's directory.

### Shell language

The shell supports:

- Variables and defaults: `NAME=value`, `$NAME`, `${NAME:-default}`, and
  `${NAME:=value}`.
- Environment operations: `export`, `unset`, and `env`.
- Command substitution: `$(command)` and backtick substitution.
- Globbing: `*.txt`, `src/*.cpp`, and recursive `**` patterns.
- Pipelines: `cat input.txt | grep error | wc -l`.
- Redirection: `>`, `>>`, and `<`.
- Conditions: `if ...; then ...; else ...; fi`.
- Loops: `for ...; do ...; done` and `while ...; do ...; done`.
- Chaining: `&&`, `||`, and `;`.
- Background jobs: `command &`, `jobs`, `fg`, and `bg`.

Examples:

```powershell
busybox sh -c 'NAME=world; echo hello $NAME'
busybox sh -c 'cat input.txt | grep error > errors.txt'
busybox sh -c 'for file in *.txt; do echo $file; done'
busybox sh -c 'sleep 10 &; jobs; fg'
```

### Text and data processing

- `base64`
- `md5sum`, `sha256sum`
- `cmp`, `diff`
- `rev`, `paste`

### File, disk, and process inspection

- `stat`, `du`, `bdf`, `ps`
- `realpath`, `readlink`, `ln`

### Existing utilities

- `grep`, `sed`, `awk`
- `test`, `[` and the core file and stream commands already shipped with the shell

## Examples

### Shell control flow

```powershell
busybox sh -c 'if test -e input.txt; then echo found; fi'
busybox sh -c 'for i in $(seq 1 5); do echo $i; done'
busybox sh -c 'while false; do echo loop; done'
```

### File and timestamp helpers

```powershell
busybox touch flag.txt
busybox touch -c existing.log
busybox date +%Y-%m-%d
busybox mktemp temp-XXX.txt
busybox mktemp -d temp-dir-XXX
```

### Process and system helpers

```powershell
busybox whoami
busybox uname -a
busybox ps
busybox kill 1234
```

### Data and comparison tools

```powershell
busybox base64 hello
busybox base64 -d aGVsbG8=
busybox sha256sum busybox.exe
busybox diff old.txt new.txt
busybox cmp file-a.bin file-b.bin
```

### Path and link helpers

```powershell
busybox realpath .\src\..\README.md
busybox readlink link.txt
busybox ln -s source.txt source-link.txt
```

### Self-test coverage

`--self-test` runs without loading the user's normal history and uses a
temporary test directory. It checks:

- Applet dispatch and sequence output.
- Assignment prefixes and shell variable state.
- Subshell, command-substitution, and pipeline cwd/environment isolation.
- Nested `exit` status handling.
- Script execution and positional arguments.
- Oversized script rejection and bounded history loading.
- Foreground job timeout enforcement.

Use the same executable that you intend to run in production:

```powershell
.\bin\busybox.exe --self-test
```

## Help and Options

- `busybox --help`: show the full applet overview.
- `busybox help <applet>`: show help for a specific applet when available.
- `busybox --version`: show version information.
- `busybox --self-test`: run the built-in shell and applet regression suite.
- `busybox <applet> --help`: many applets also accept direct help flags.

## Notes

- `sh`, `bash`, and `ksh` map to the same shell engine in this repo.
- Some applets are lightweight Windows-native approximations of their Unix counterparts.
- Shell scripts should prefer `seq`, `touch`, `mktemp`, `date`, and `test` for portable automation patterns.
- Script input, command-substitution output, history loading, glob expansion,
  and recursive `find` traversal are bounded to prevent accidental resource exhaustion.

## UNIX Origin

This project is inspired by embedded Unix-style toolchains and busybox-based Linux environments, but it is implemented as a Windows-native utility suite in this repository.
