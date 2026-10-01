# supervisord

## What it does

Runs a native Windows process supervisor that starts, monitors, and controls configured programs.

It can auto-restart failed processes, run as a console daemon or Windows service, and accept control commands over a local named pipe.

## Usage

```text
supervisord.exe [--help|-h|help]
supervisord.exe [--check-config [path]]
supervisord.exe [--service]
supervisord.exe [--install-service|--uninstall-service]
supervisord.exe <status|diag|metrics|reload|start|stop|restart> [arg]
```

## Commands

- `--help`, `-h`, `help`: show help text
- `--check-config [path]`: validate config and print warnings/errors
- `--service`: run under Windows Service Control Manager (SCM)
- `--install-service`: install service name `Supervisord` with auto-start
- `--uninstall-service`: remove service name `Supervisord`
- `status`: show state of all managed processes
- `diag [limit]`: print recent in-memory supervisor events
- `metrics`: print runtime metrics as JSON
- `reload`: reload config and apply changes in-place (admin required)
- `start <process_name>`: start one configured process
- `stop <process_name>`: stop one managed process
- `restart <process_name>`: stop then start one managed process

## Configuration

Config file path:

```text
<executable_dir>\supervisord.conf
```

Named pipe for command client/server:

```text
\\.\pipe\supervisord
```

Program section format:

```ini
[program:<name>]
command=<command line>
directory=<working dir>
depends_on=api,db
shutdown_phase=10
autostart=true|false
autorestart=true|false|unexpected
exitcodes=0,2
startretries=3
stoptimeout=10
stop_command=<command line>
healthcheck_command=cmd /d /c "curl -f http://localhost:8080/health"
healthcheck_interval=30
healthcheck_failures=3
stdout_logfile=<path>
stdout_logfile_maxbytes=10485760
stdout_logfile_backups=5
stderr_logfile=<path>
stderr_logfile_maxbytes=10485760
stderr_logfile_backups=5
environment=KEY=VALUE,KEY2=VALUE2
```

## Minimal Example Config

```ini
[program:api]
command=C:\apps\api\api.exe --port 8080
directory=C:\apps\api
autostart=true
autorestart=unexpected
exitcodes=0
startretries=3
stoptimeout=10
stop_command=cmd /d /c "echo stop api"
shutdown_phase=20
healthcheck_command=cmd /d /c "curl -f http://localhost:8080/health"
healthcheck_interval=20
healthcheck_failures=3
stdout_logfile=C:\logs\api.out.log
stderr_logfile=C:\logs\api.err.log
environment=ASPNETCORE_ENVIRONMENT=Production

[program:worker]
command=C:\apps\worker\worker.exe
directory=C:\apps\worker
autostart=true
autorestart=true
depends_on=api
shutdown_phase=10
exitcodes=0
startretries=5
stoptimeout=15
stdout_logfile=C:\logs\worker.out.log
stderr_logfile=C:\logs\worker.err.log
```

## Examples

```text
# 1) Validate config in CI
supervisord.exe --check-config

# 2) Run as a console daemon
supervisord.exe

# 3) Show all managed process states
supervisord.exe status

# 4) Show runtime metrics
supervisord.exe metrics

# 5) Reload config without full daemon restart
supervisord.exe reload

# 6) Restart one configured process
supervisord.exe restart api

# 7) Install and start as a Windows service
supervisord.exe --install-service
sc start Supervisord

# 8) Stop and uninstall service
sc stop Supervisord
supervisord.exe --uninstall-service
```

## Notes

- If no arguments are passed, `supervisord` starts as a console daemon.
- Client commands (`status`, `start`, `stop`, `restart`) require a running daemon instance.
- `--service` is used by the SCM service entry and is usually not typed manually.
- Service display name is `Windows Native Supervisor Service`.
- Control commands return non-zero exit codes on IPC/daemon/protocol errors.
- Mutating commands (`start`, `stop`, `restart`, `reload`) require admin privileges.
- Configuration loading reports line-scoped warnings and errors; use `--check-config` in CI.
- `stop_command` is an optional best-effort pre-stop hook run before break/terminate signaling.
- Startup order honors `depends_on`; shutdown order is deterministic with reverse dependency order plus descending `shutdown_phase`.
- Health checks are optional and can force restart policy actions if repeated failures occur.

## UNIX origin

Inspired by supervisor-style process managers commonly used on UNIX-like systems, adapted to native Windows APIs and service management.
