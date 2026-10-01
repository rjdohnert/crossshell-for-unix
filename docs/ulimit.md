# ulimit

## What it does
Reports the current process resource snapshot or launches a child command under Windows job-object limits.

## Options
- `-a`: print a resource snapshot and the supported limit knobs
- `-m MB`: limit per-process memory usage for a child command
- `-t SECONDS`: limit per-process CPU time for a child command
- `-u N`: limit the active process count for a child command
- `-h`, `--help`: show help text
- `--version`: show version information

## Behavior
- Without a child command, `ulimit` prints the current process working set, peak working set, private usage, and CPU time.
- With a child command, it starts that command in a Windows job object and applies the selected limits before running it.
- If no limits are supplied, it still runs the child command normally.

## Examples
```text
ulimit -a
ulimit -m 512 -- myapp.exe
ulimit -t 30 -- cmd.exe /c heavy_task.cmd
```

## UNIX origin
The name comes from the Unix shell built-in for viewing and changing resource limits. This Windows version maps the idea to job-object process limits and process resource reporting.