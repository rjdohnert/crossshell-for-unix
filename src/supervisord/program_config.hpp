#pragma once

#include "supervisor_defaults.hpp"
#include "supervisord.hpp"

struct ProgramConfig {
    std::wstring name;
    std::wstring command;
    std::wstring directory;
    std::vector<std::wstring> depends_on;
    int shutdown_phase = 0;
    bool autostart = true;
    std::wstring autorestart = L"unexpected";
    std::vector<DWORD> exitcodes = {0};
    int startretries = SupervisorDefaults::kDefaultStartRetries;
    int stoptimeout = SupervisorDefaults::kDefaultStopTimeoutSeconds;
    std::wstring stop_command;
    std::wstring healthcheck_command;
    int healthcheck_interval = 0;
    int healthcheck_failures = SupervisorDefaults::kDefaultHealthFailures;
    std::wstring stdout_logfile;
    size_t stdout_logfile_maxbytes = SupervisorDefaults::kDefaultLogMaxBytes;
    int stdout_logfile_backups = SupervisorDefaults::kDefaultLogBackups;
    std::wstring stderr_logfile;
    size_t stderr_logfile_maxbytes = SupervisorDefaults::kDefaultLogMaxBytes;
    int stderr_logfile_backups = SupervisorDefaults::kDefaultLogBackups;
    std::map<std::wstring, std::wstring> environment;
};
