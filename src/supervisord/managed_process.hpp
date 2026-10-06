#pragma once

#include "process_state.hpp"
#include "program_config.hpp"
#include "scoped_handle.hpp"
#include "supervisord.hpp"

class ManagedProcess {
public:
    // ManagedProcess public surface:
    // - Start/Stop are lifecycle-safe and idempotent under internal mutexes.
    // - OnSharedHealthCheckTick is called by shared scheduler and must remain non-blocking.
    // - Config is immutable after construction; reload replaces whole instance.
    static inline std::atomic<unsigned long long> g_totalStarts{0};
    static inline std::atomic<unsigned long long> g_totalLaunchFailures{0};
    static inline std::atomic<unsigned long long> g_totalUnexpectedExits{0};
    static inline std::atomic<unsigned long long> g_totalRestarts{0};
    static inline std::atomic<unsigned long long> g_totalHealthCheckFailures{0};

    ProgramConfig config;

    ManagedProcess(const ProgramConfig& cfg);

    ~ManagedProcess();

    void Start();

    void Stop();

    ProcessState GetState() const;
    std::wstring GetName() const;

private:
    std::atomic<ProcessState> state_;
    std::atomic<bool> should_run_{false};
    std::atomic<bool> stop_health_{false};
    std::atomic<bool> health_registered_{false};
    std::atomic<int> health_failures_{0};
    int retry_count_ = 0;

    std::thread worker_thread_;
    std::mutex mtx_;
    std::mutex lifecycle_mtx_;

    ScopedHandle hProcess_;
    ScopedHandle hJob_;
    ScopedHandle hStdOutRead_;
    ScopedHandle hStdErrRead_;

    void JoinWorkerThread();

    void RequestHealthStop();

    void StartPipeLogging(HANDLE hReadPipe, const std::wstring& path, size_t maxBytes, int backups);

    void StopPipeLogging();

    void StopHealthMonitor();

    void Log(const std::string& msg);

    void CreateJob();

    bool RunBestEffortStopCommand();

    int ComputeBackoffMs(int attempt);

    bool RunHealthCheckOnce();

    void StartHealthMonitor();

public:
    void OnSharedHealthCheckTick();

private:

    bool Launch();

    void KillProcessTree();

    void RunLoop();
};
