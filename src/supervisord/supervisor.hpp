#pragma once

#include "managed_process.hpp"
#include "metrics_snapshot.hpp"
#include "status_snapshot.hpp"
#include "supervisord.hpp"

class Supervisord {
public:
    // Supervisord public surface:
    // - Owns process registry, IPC command execution, and lifecycle orchestration.
    // - Configuration reload applies add/update/remove atomically under supervisor lock.
    // - IPC handlers return stable framed protocol responses.
    static inline std::atomic<bool> g_SignalReceived{false};
    static inline std::atomic<unsigned long long> g_IpcRequests{0};
    static inline std::atomic<unsigned long long> g_IpcErrors{0};
    static inline std::atomic<unsigned long long> g_ReloadCount{0};

    bool LoadConfiguration(const std::wstring& configFile);

    bool ReloadConfiguration(std::string& summary);

    std::vector<std::wstring> BuildDependencyOrderLocked();

    void StartAll();

    void Shutdown();

    MetricsSnapshot BuildMetricsSnapshot();

    std::string RenderMetricsJson(const MetricsSnapshot& snapshot) const;

    std::vector<StatusSnapshotEntry> BuildStatusSnapshot();

    std::string RenderStatusSnapshot(const std::vector<StatusSnapshotEntry>& snapshot) const;

    std::string BuildMetricsJson();

    void WriteMetricsFile();

    std::mutex& GetProcessCommandLock(const std::wstring& processName);

    // Lock ordering policy:
    // 1) process-specific command mutex from GetProcessCommandLock
    // 2) supervisor_mtx_
    bool ExecuteWithProcessLocked(const std::wstring& processName, const std::function<void(ManagedProcess&)>& fn);

    std::string HandleIpcCommand(const std::string& cmd, bool clientPrivileged);

    void RunIpcServer();

    void JoinIpcServer();

private:
    std::mutex supervisor_mtx_;
    std::map<std::wstring, std::unique_ptr<ManagedProcess>> processes_;
    std::mutex process_lock_map_mtx_;
    std::map<std::wstring, std::unique_ptr<std::mutex>> process_locks_;
    std::wstring config_path_;
    std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::now();
    std::thread ipc_thread_;
};
