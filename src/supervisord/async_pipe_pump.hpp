#pragma once

#include "rotating_file_sink.hpp"
#include "supervisor_defaults.hpp"
#include "supervisord.hpp"

class AsyncPipePump {
public:
    static AsyncPipePump& Instance();

    bool RegisterPipe(HANDLE sourceReadHandle, const std::wstring& path, size_t maxBytes, int backups);

    void UnregisterPipe(HANDLE sourceReadHandle);

private:
    struct PipeContext {
        uintptr_t sourceKey = 0;
        HANDLE readHandle = INVALID_HANDLE_VALUE;
        OVERLAPPED ov = {0};
        std::array<char, 4096> buffer{};
        std::unique_ptr<RotatingFileSink> sink;
        std::atomic<bool> stopping{false};
        std::atomic<bool> retired{false};
    };

    HANDLE iocp_ = NULL;
    std::vector<std::thread> workers_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::unordered_map<uintptr_t, std::shared_ptr<PipeContext>> bySource_;
    std::unordered_map<OVERLAPPED*, std::shared_ptr<PipeContext>> byOverlapped_;

    AsyncPipePump();

    ~AsyncPipePump();

    bool IssueRead(const std::shared_ptr<PipeContext>& ctx);

    void WorkerLoop();

    void RetireContext(const std::shared_ptr<PipeContext>& ctx);
};
