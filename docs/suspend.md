# suspend

## What it does
Suspends or resumes one or more processes by PID on Windows.

## Options
- `-r`: resume the target process instead of suspending it
- `-h`, `--help`: show help text
- `--version`: show version information

## Notes
- This utility operates on process IDs, not shell job names.
- It uses Windows process/thread suspension APIs and falls back to thread-by-thread suspension if needed.
- It refuses to target its own process for safety.

## Examples
```text
suspend 1234
suspend -r 1234
```

## UNIX origin
This follows the general idea of a shell-control command that pauses execution, adapted for Windows process control.