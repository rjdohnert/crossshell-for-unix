# w

## What it does
Shows who is logged in and what each session is doing, similar to the Unix `w` command.

This implementation reports the current system summary, active user sessions, terminal names, login times, idle times, CPU usage, and the foreground command when available.

## Usage
```text
w [options] [user]
```

## Options
- `-h`, `--no-header`: suppress the system summary header line
- `-s`, `--short`: print a shorter table without `Login Time`, `JCPU`, and `PCPU`
- `-f`, `--from`: include the `FROM` column with remote client host or IP information
- `-l`, `--long`: force the long table format
- `-?`, `--help`: show help text

## Output Columns
- `User`: the logged-in Windows account name
- `TTY`: the terminal station name, such as `Console` or an RDP session name
- `FROM`: the remote client address or hostname
- `Login@`: when the session logged in
- `Idle`: how long the session has been inactive
- `JCPU`: total CPU time for the session
- `PCPU`: CPU time for the foreground process
- `WHAT`: the active command or process name

## Examples
- `w`
- `w -s`
- `w -f`
- `w Administrator`

## Notes
- The header shows uptime, active user count, and approximate CPU usage.
- `-h` in this implementation means "no header", not help.
- `-?` or `--help` displays the help screen.

## UNIX origin
A classic Unix session-monitoring utility from BSD and System V systems.