#pragma once

#include "async_pipe_pump.hpp"
#include "managed_process.hpp"
#include "supervisord.hpp"

class ManagedProcess;

class HealthCheckScheduler {
public:
    static HealthCheckScheduler& Instance();
    void RegisterProcess(ManagedProcess* process, int intervalSeconds);
    void UnregisterProcess(ManagedProcess* process);
};

// AsyncPipePump public surface:
// - RegisterPipe/UnregisterPipe are thread-safe and keyed by original read handle.
// - Uses IOCP + fixed worker pool for scalable non-blocking log draining.
// - Retires contexts idempotently and closes duplicated async handles internally.
