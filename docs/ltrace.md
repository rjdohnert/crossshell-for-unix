# ltrace

## What it does

Intercepts and logs dynamic link library (DLL) function calls made by a process using software breakpoints and the Windows debug API.

## Usage

- `ltrace [OPTIONS] -- <command> [args...]`
- `ltrace [OPTIONS] -p <pid>`

## Options

- `-p`, `--pid <PID>`: attach to an existing running process by Process ID
- `-l`, `--lib <MODULE>`: filter tracing to a specific DLL module (repeatable)
- `-o`, `--output <FILE>`: write trace log output to a file
- `-v`, `--verbose`: print debug logging regarding symbol hooking
- `--all-exports`: disable the default unfiltered export hook cap
- `--no-color`: disable ANSI color output
- `-h`, `--help`: print the help screen
- `-V`, `--version`: print version information

## Notes

- Without `-l`, ltrace hooks up to 512 exports per module to avoid excessive overhead.
- Use `--all-exports` to disable the cap; most useful when `-l` narrows the module set.
- The `--` separator is required when passing a command with its own arguments.
- Tracing attaches via `DEBUG_ONLY_THIS_PROCESS`; child processes are not followed.
- Each traced call displays the module name, function name, and up to four dereferenced arguments.
- String arguments are inspected as ANSI or UTF-16 wide strings where memory is readable.

## Examples

- `ltrace -- notepad.exe`
- `ltrace -l kernel32 -l user32 -- notepad.exe`
- `ltrace -p 4812`
- `ltrace -o trace_log.txt -l ntdll -- ping.exe 127.0.0.1`
- `ltrace -v --all-exports -l ntdll -- cmd.exe`
- `ltrace --help`
- `ltrace --version`

## UNIX origin

A Unix/Linux debugging utility; on Linux it intercepts shared library calls via `ptrace`. This implementation uses the Windows debug API with INT3 software breakpoints.
