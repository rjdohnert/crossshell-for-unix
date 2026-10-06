#pragma once

#include "supervisor_defaults.hpp"
#include "supervisord.hpp"

class RotatingFileSink {
    static inline std::mutex g_rotateGlobalMtx;
    static inline std::atomic<unsigned long long> g_writeFailures{0};
    static inline std::atomic<unsigned long long> g_rotateFailures{0};
    static inline std::atomic<unsigned long long> g_openFailures{0};
    static inline std::atomic<unsigned long long> g_droppedBytes{0};
    std::wstring filePath_;
    size_t maxBytes_;
    int maxBackups_;
    HANDLE hFile_ = INVALID_HANDLE_VALUE;
    size_t currentSize_ = 0;
    std::mutex mtx_;

public:
    RotatingFileSink(std::wstring path, size_t maxBytes = SupervisorDefaults::kDefaultLogMaxBytes, int maxBackups = SupervisorDefaults::kDefaultLogBackups);

    ~RotatingFileSink();

    static void ReportStats();

    void Write(const char* data, DWORD size);

private:
    void CloseFile();

    void OpenFileIfNeeded();

    void RotateIfNeeded(DWORD incomingBytes);
};
