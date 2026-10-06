#pragma once

#include "supervisord.hpp"

struct MetricsSnapshot {
    long long uptime_seconds = 0;
    unsigned long long ipc_requests = 0;
    unsigned long long ipc_errors = 0;
    unsigned long long reload_count = 0;
    unsigned long long process_starts = 0;
    unsigned long long launch_failures = 0;
    unsigned long long unexpected_exits = 0;
    unsigned long long restarts = 0;
    unsigned long long healthcheck_failures = 0;
};
