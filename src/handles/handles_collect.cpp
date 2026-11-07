#define WIN32_LEAN_AND_MEAN
#include "handles_collect.hpp"

#include <tlhelp32.h>

#include <utility>

namespace {
class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE handle = INVALID_HANDLE_VALUE)
        : m_handle(handle) {}
    ~ScopedHandle() {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr) {
            CloseHandle(m_handle);
        }
    }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            close();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    HANDLE get() const {
        return m_handle;
    }

    bool valid() const {
        return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr;
    }

private:
    void close() {
        if (valid()) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

    HANDLE m_handle;
};

std::wstring WindowsError(DWORD code) {
    wchar_t* buffer = nullptr;
    const DWORD length = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr, code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    std::wstring message = length != 0 && buffer != nullptr
        ? std::wstring(buffer, length)
        : L"Windows error " + std::to_wstring(code);
    if (buffer != nullptr) LocalFree(buffer);
    while (!message.empty() &&
           (message.back() == L'\r' || message.back() == L'\n' ||
            message.back() == L' ')) {
        message.pop_back();
    }
    return message;
}

struct ProcessEntry {
    DWORD pid;
    std::wstring name;
};

bool SnapshotProcesses(std::vector<ProcessEntry>& entries,
                       std::wstring& error) {
    ScopedHandle snapshot;
    for (unsigned int attempt = 0; attempt < 8; ++attempt) {
        snapshot = ScopedHandle(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
        if (snapshot.valid()) break;
        const DWORD code = GetLastError();
        if (code != ERROR_BAD_LENGTH || attempt == 7) {
            error = L"cannot create process snapshot: " + WindowsError(code);
            return false;
        }
        snapshot = ScopedHandle();
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (!Process32FirstW(snapshot.get(), &entry)) {
        const DWORD code = GetLastError();
        if (code == ERROR_NO_MORE_FILES) return true;
        error = L"cannot read process snapshot: " + WindowsError(code);
        return false;
    }

    do {
        entries.push_back({entry.th32ProcessID, entry.szExeFile});
        entry.dwSize = sizeof(entry);
        SetLastError(ERROR_SUCCESS);
    } while (Process32NextW(snapshot.get(), &entry));

    const DWORD code = GetLastError();
    if (code != ERROR_NO_MORE_FILES) {
        error = L"cannot finish reading process snapshot: " + WindowsError(code);
        return false;
    }
    return true;
}

HandleCountStatus StatusForError(DWORD code) {
    return code == ERROR_ACCESS_DENIED
        ? HandleCountStatus::accessDenied
        : HandleCountStatus::unavailable;
}
}

bool HandlesCollector::collect(std::vector<ProcessHandleInfo>& processes,
                               std::wstring& error) {
    processes.clear();
    error.clear();

    std::vector<ProcessEntry> entries;
    if (!SnapshotProcesses(entries, error)) return false;

    const DWORD currentPid = GetCurrentProcessId();
    processes.reserve(entries.size());
    for (const ProcessEntry& entry : entries) {
        ProcessHandleInfo info;
        info.pid = entry.pid;
        info.name = entry.name;

        if (entry.pid == 0) {
            info.status = HandleCountStatus::unavailable;
            processes.push_back(std::move(info));
            continue;
        }

        ScopedHandle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                                         FALSE, entry.pid));
        if (!process.valid()) {
            info.status = StatusForError(GetLastError());
            processes.push_back(std::move(info));
            continue;
        }

        DWORD count = 0;
        if (!GetProcessHandleCount(process.get(), &count)) {
            info.status = StatusForError(GetLastError());
            processes.push_back(std::move(info));
            continue;
        }
        if (entry.pid == currentPid && count != 0) --count;

        info.handleCount = count;
        info.status = HandleCountStatus::available;
        processes.push_back(std::move(info));
    }
    return true;
}
