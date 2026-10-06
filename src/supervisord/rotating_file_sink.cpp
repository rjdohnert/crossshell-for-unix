#include "logger.hpp"
#include "rotating_file_sink.hpp"
#include "supervisor_defaults.hpp"

RotatingFileSink::RotatingFileSink(std::wstring path, size_t maxBytes , int maxBackups )
        : filePath_(std::move(path)), maxBytes_(maxBytes), maxBackups_(maxBackups) {
        OpenFileIfNeeded();
    }

RotatingFileSink::~RotatingFileSink() {
        CloseFile();
    }

void RotatingFileSink::ReportStats() {
        const auto writeFailures = g_writeFailures.load();
        const auto rotateFailures = g_rotateFailures.load();
        const auto openFailures = g_openFailures.load();
        const auto dropped = g_droppedBytes.load();
        if (writeFailures == 0 && rotateFailures == 0 && openFailures == 0 && dropped == 0) return;

        Logger::Log(
            "supervisord",
            "logsink stats: write_failures=" + std::to_string(writeFailures) +
            " rotate_failures=" + std::to_string(rotateFailures) +
            " open_failures=" + std::to_string(openFailures) +
            " dropped_bytes=" + std::to_string(dropped)
        );
    }

void RotatingFileSink::Write(const char* data, DWORD size) {
        if (filePath_.empty() || filePath_ == L"NONE") return;
        std::lock_guard<std::mutex> lock(mtx_);

        OpenFileIfNeeded();
        if (hFile_ == INVALID_HANDLE_VALUE) {
            g_droppedBytes += size;
            return;
        }

        RotateIfNeeded(size);

        DWORD bytesWritten = 0;
        if (WriteFile(hFile_, data, size, &bytesWritten, NULL)) {
            currentSize_ += bytesWritten;
        } else {
            g_writeFailures++;
            g_droppedBytes += size;
        }
    }

void RotatingFileSink::CloseFile() {
        if (hFile_ != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile_);
            hFile_ = INVALID_HANDLE_VALUE;
        }
    }

void RotatingFileSink::OpenFileIfNeeded() {
        if (filePath_.empty() || filePath_ == L"NONE" || hFile_ != INVALID_HANDLE_VALUE) return;

        try {
            currentSize_ = fs::exists(filePath_) ? static_cast<size_t>(fs::file_size(filePath_)) : 0;
        } catch (...) {
            currentSize_ = 0;
        }

        // Allow other compatible handles during rotation/reopen on Windows.
        hFile_ = CreateFileW(
            filePath_.c_str(),
            FILE_APPEND_DATA,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );
        if (hFile_ == INVALID_HANDLE_VALUE) {
            g_openFailures++;
        }
    }

void RotatingFileSink::RotateIfNeeded(DWORD incomingBytes) {
        if (maxBytes_ == 0 || (currentSize_ + incomingBytes < maxBytes_)) return;

        std::lock_guard<std::mutex> globalRotateLock(g_rotateGlobalMtx);

        CloseFile();

        try {
            if (fs::exists(filePath_)) {
                for (int i = maxBackups_ - 1; i >= 1; --i) {
                    fs::path oldFile = filePath_ + L"." + std::to_wstring(i);
                    fs::path newFile = filePath_ + L"." + std::to_wstring(i + 1);
                    if (fs::exists(oldFile)) {
                        fs::rename(oldFile, newFile);
                    }
                }
                if (maxBackups_ > 0) {
                    fs::rename(filePath_, filePath_ + L".1");
                } else {
                    fs::remove(filePath_);
                }
            }
        } catch (...) {
            g_rotateFailures++;
        }

        currentSize_ = 0;
        OpenFileIfNeeded();
    }
