#include "supervisor.hpp"
#include "supervisord_help.hpp"

void PrintHelp() {
    std::cout << R"(supervisord(8)          CrossShell for UNIX Reference Manual          supervisord(8)

    NAME
        supervisord - enterprise process supervisor and service daemon

    SYNOPSIS
        supervisord [OPTIONS]
        supervisord COMMAND [ARGUMENTS...]

    DESCRIPTION
        supervisord is a process control system that enables users to monitor
        and control a number of processes on Windows. It can run as an
        interactive console daemon or as a background Windows Service. Client
        control commands interact with the running daemon over a local Win32
        named pipe (\\.\pipe\supervisord).

    COMMANDS
        status
            Display the current state and uptime of all managed processes.

        start PROCESS
            Start a configured managed process by name.

        stop PROCESS
            Stop a running managed process by name.

        restart PROCESS
            Stop and immediately restart a managed process by name.

        reload
            Reload configuration file in-place and synchronize processes.

        diag [LIMIT]
            Display recent in-memory supervisor event history (default 50).

        metrics
            Export runtime supervisor metrics formatted as JSON.

    OPTIONS
        --check-config [PATH]
            Validate configuration syntax and report warnings and errors.
            Default path: %USERPROFILE%\supervisord.conf.

        --service
            Run under the Windows Service Control Manager (SCM).

        --install-service
            Register and install 'Supervisord' as an automatic Windows service.

        --uninstall-service
            Stop and unregister the 'Supervisord' Windows service.

        -h, --help, help
            Display this reference manual.

        -V, --version
            Display version and license information.

    CONFIGURATION FILE FORMAT
        The configuration file (%USERPROFILE%\supervisord.conf) uses standard
        INI section syntax:

            [program:name]
            command=executable.exe --arg
            directory=C:\path\to\workdir
            autostart=true|false
            autorestart=true|false|unexpected
            exitcodes=0,2
            startretries=3
            stoptimeout=10
            stop_command=graceful_stop.exe
            stdout_logfile=C:\logs\out.log
            stderr_logfile=C:\logs\err.log
            environment=KEY=VALUE,KEY2=VALUE2

    EXAMPLES
        supervisord
            Start supervisord as an interactive foreground console daemon.

        supervisord --check-config
            Verify %USERPROFILE%\supervisord.conf syntax.

        supervisord status
            Query the status of all processes from the running daemon.

        supervisord restart web_worker
            Restart the process named 'web_worker'.

        supervisord --install-service
            Install supervisord as a native Windows service.

    CrossShell for UNIX                                                supervisord(8)
)";
}

void PrintVersion() {
    std::cout << "supervisord (CrossShell) 5.0.0\n"
              << "Copyright (c) 2026 Roberto J Dohnert. All rights reserved.\n";
}
