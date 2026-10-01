# strace

## What it does
Launches a target program under the Windows debugger and traces process creation, threads, DLL loads, exceptions, and debug output.

## Usage
- `strace [OPTION]... COMMAND [ARG]...`

## Options
- `-f`, `--follow`: trace child processes created by the target
- `-F`: alias for `--follow`
- `-p`, `--pid PID`: attach to an existing process by PID instead of launching a new one
- `-tt`, `--timestamp`: prefix each event line with a local timestamp
- `-v`, `--verbose`: print additional detail for selected events
- `-qq`: suppress informational banner lines and only show traced events
- `-e`, `--events LIST`: restrict output to selected event classes such as `process`, `thread`, `dll`, `exception`, or `debug`
- `-o`, `--output FILE`: write trace output to a file instead of stdout
- `--help`: show help text and exit
- `--version`: show version information and exit

## Notes
- `strace` is best used with native Windows executables.
- Use `cmd /c ...` when you need to run shell commands or pipelines.
- The trace output includes process creation events, thread activity, DLL loading, debug strings, and exceptions.
- The output can be filtered to reduce noise and timestamped for easier correlation.

## Examples
- `strace cmd /c dir`
- `strace -f -tt cmd /c dir`
- `strace -F -qq -e process,thread notepad.exe`
- `strace -p 1234 -tt -e process,thread`
- `strace -e process,dll -v notepad.exe`
- `strace -o trace.txt notepad.exe`
- `strace --help`
- `strace --version`

## UNIX origin
A Unix/Linux debugging utility rather than a single classic BSD or System V command.
