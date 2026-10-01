# ksh

## What it does

Provides a KornShell-style interactive shell with scripting features, built-ins, history, jobs, aliases, functions, variables, arithmetic, redirection, and startup profile support.

This shell is the main scripting environment used by the repo and includes a large compatibility surface for traditional Unix shell workflows.

## Usage

```text
ksh [options] [script [args...]]
ksh -c "command" [name [args...]]
ksh --script-test SCRIPT [args...]
ksh -- [script [args...]]
```

## Options

- `-h`, `--help`: show help text and list built-in commands
- `--version`: show version information
- `--no-profile`: skip loading the startup profile
- `--profile FILE`: load a specific startup profile
- `-c COMMAND`: execute a command string and exit with its status; an optional name and arguments provide `$0` and positional parameters
- `--script-test SCRIPT [args...]`: run a script without loading a startup profile, report `PASS` or `FAIL`, and return the script's exit status
- `--`: stop option parsing and treat the remaining arguments as a script name and its arguments

## Shell Syntax

```text
command1 | command2             Pipeline standard output into the next command
command &                       Run a command or pipeline in the background
<(command) / >(command)         Named-pipe process substitution
< file, > file, >> file         Standard input/output redirection
2> file, 2>> file               Standard error redirection
2>&1, 1>&2                      Duplicate output/error streams
command1; command2              Execute top-level commands in sequence
command |                       Continue a pipeline on the next script line
backslash at line end            Continue a logical command on the next line
```

Comments begin with `#` outside quotes. Single and double quotes preserve spaces and protect shell metacharacters according to the shell's quoting rules.

## Expansion

- `$name` and `${name}` expand scalar variables.
- `${array[index]}` expands indexed or associative array elements.
- `$(command)` captures command substitution output.
- `$((expression))` evaluates arithmetic expressions.
- `$?` is the previous command status; `0` means success.
- `HKLM` and `HKCU` are dynamic Registry namespaces. Read values with `${HKLM.Key.Path.Value}` or use `/` as the Registry path separator when key names contain literal periods, for example `${HKLM.Software/Microsoft/.NETFramework.InstallRoot}`.
- Registry assignments update existing `REG_SZ`, `REG_EXPAND_SZ`, `REG_DWORD`, and `REG_QWORD` values. Unsupported Registry types are rejected.

## Control Flow

The shell supports `if`/`elif`/`else`/`fi`, `for`/`do`/`done`, `while`, `until`, `select`, and `case`/`in`/`esac` blocks. `break` and `continue` accept an optional positive loop-depth argument.

```ksh
if test -n "$value"; then
	echo set
else
	echo unset
fi

for x in 1 2 3; do echo "$x"; done

case "$value" in
	yes) echo true ;;
	*) echo false ;;
esac
```

Scripts may use semicolon-separated command lists, escaped continuations, and pipelines continued on the next physical line. Builtin and function pipeline stages run with isolated shell state; external commands inherit exported variables.

## Built-in Areas

- Shell control: `exit`, `logout`, `builtin`, `command`, `whence`, `type`, `eval`, `source`, `.`, `exec`, `stty`
- Directory navigation: `cd`, `pwd`, `dirs`, `pushd`, `popd`
- Variables and expansion: `set`, `unset`, `export`, `readonly`, `typeset`, `enum`, `getopts`, `alias`, `unalias`, `hash`, `let`, `return`, `[[`, `test`, `[` 
- Jobs and signals: `jobs`, `fg`, `bg`, `disown`, `kill`, `wait`, `trap`
- Files and system: `find`, `pathchk`, `getconf`, `umask`, `times`, `sleep`, `shift`, `:`

## Enumerations and Timing

- `enum Name=(value ...)` creates an integer custom type with members numbered from `0`.
- `sleep <seconds>` pauses for a non-negative decimal duration, with millisecond precision. Ctrl+C, Ctrl+Break, HUP, and TERM traps interrupt the wait promptly.

```text
enum Status_t=(running stopped error)
Status_t state
print ${state.running}
sleep 0.250
```

## Arithmetic

Arithmetic expansion supports `**` for exponentiation, C/ksh-style `^` bitwise XOR,
shifts, relational/equality comparisons, bitwise and logical operators, and the
ternary conditional operator.

```text
print $((2 ** 3))
print $((5 ^ 3))
print $((1 < 2 ? 10 : 20))
```
- Interactive and diagnostics: `print`, `echo`, `printf`, `true`, `false`, `history`, `complete`, `fc`, `clear`, `math`, `read`

## Control Flow

The script engine recognizes `break` and `continue` inside `for`, `while`, `until`, and `select` blocks. Both accept an optional positive integer to target outer loop nesting.

## External Utilities

The shell help also points to standalone process-control utilities that live alongside `ksh`:

- `suspend`: suspend or resume a process by PID
- `ulimit`: report resource usage or launch a child under Windows job limits

## Prompt Customization

The shell supports `PS1` customization in `~/.kshrc`.

Supported prompt tokens:

- `%u`: user name
- `%d`: domain
- `%w`: current working directory
- `%m`: host name
- `%#`: role character (`$` for normal users, `#` for admin sessions)
- `%%`: literal percent

Examples:

```text
PS1='%u@%d %w %# '
PS1='%w %# '
```

## Line Editing (vi mode)

Interactive input supports a vi-style mode toggle through `set`:

- `set -o vi`: enable vi editing mode
- `set +o vi`: disable vi editing mode (default behavior)

In vi mode:

- Press `Esc` to enter command mode.
- Press `i`, `a`, or `A` to return to insert mode, or `I` to insert at the first non-blank character.
- Use `0` to jump to column 1, and `^` or `_` to jump to the first non-blank character.
- Use `h` and `l` to move the cursor.
- Use `|` to jump to column 1, or `N|` to jump to 1-based column `N`.
- Use `w`/`W` and `b`/`B` to jump by words, `e`/`E` to move to word end, and `ge`/`gE` to move backward to the previous word end.
- Use `f`/`F` to find a character on the line, `t`/`T` to move just before/after a found character, `;` to repeat the last find motion, and `,` to repeat it in the opposite direction.
- Prefix movement keys with a count, for example `3h`, `4l`, `2w`, `2b`, `3e`, `2fX`, `4;`, `8|`, and history jumps like `2k` / `3j`.
- Use `x` and `D` to delete text from the current cursor position.
- `k` and `j` (and Up/Down arrows) navigate command history.

When command mode is active, cursor movement follows vi-style bounds and does not move past the last character.

## Examples

- `ksh`
- `ksh script.ksh`
- `ksh --no-profile`
- `ksh --profile C:\Users\me\.kshrc`
- `ksh -c "echo hello"`

## Notes

- `cd` accepts both Windows paths and Unix-style rooted paths.
- `dirs` prints the current directory followed by the saved directory stack.
- `pushd` pushes the current directory before changing to a new one.
- `popd` restores the most recently pushed directory.
- `true` returns success and `false` returns failure.
- The shell exposes built-in help for many commands through `help <command>`.
- It supports startup profile loading, aliases, functions, job control, and redirection handling.
- Interactive command history is stored in the user home directory as `~/.ksh_history` (on Windows this resolves from `HOME`, then `USERPROFILE`, then `HOMEDRIVE`+`HOMEPATH`).
- History entries are persisted with timestamps using `YYYY-MM-DD HH:MM:SS<TAB>command` format.
- Process substitution uses a bounded connect wait controlled by `ksh_PROC_SUB_CONNECT_TIMEOUT_MS`.
- Default connect timeout is `120000` ms, clamped to `1000`..`600000` ms when overridden.
- Script parsing accepts semicolon-separated command lists, escaped and trailing-pipe continuations, and compact `for ...; do ...; done` blocks.
- Script files are limited to 64 MiB. Shell variables, functions, aliases, history entries, array indices, and function nesting are bounded to prevent unbounded resource growth.
- `source` and `.` execute in the current shell state; a script passed as the positional command runs as the shell's script entrypoint.
- `--script-test` prints a clear `PASS` or `FAIL` result and preserves the script's final status when it is numeric.

## Exit Status and Signals

- The shell exits with the last command status, or the status supplied to `exit`.
- `0` indicates success; non-zero values indicate failure or interruption.
- `trap` supports `EXIT`, `HUP`, `INT`, `BREAK`, `TERM`, `CHLD`, and related signal names exposed by the Windows console/runtime integration.
- `sleep` accepts decimal seconds with millisecond precision and responds to pending interrupt and termination traps.

## Known Divergences from AT&T ksh93

- This is a Windows-native shell: process groups, terminal control, signals, and job control are emulated through Win32 APIs and are not full POSIX equivalents.
- `cd` and path-sensitive built-ins accept Windows paths in addition to Unix-style paths; external programs retain their own Windows command-line parsing rules.
- Custom descriptors `3` through `9` are currently supported as persistent `exec` redirections only; arbitrary-command descriptor redirection is rejected.
- Process substitution is implemented with Windows named pipes and relay threads, so it does not provide Unix FIFO semantics in every edge case.

## UNIX origin

A KornShell-compatible command interpreter inspired by AT&T KornShell and later POSIX shell behavior.
