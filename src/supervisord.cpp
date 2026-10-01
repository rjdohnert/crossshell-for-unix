/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Single-File Index (maintainability map)
 *
 *  1) Platform setup and shared IPC framing helpers
 *     - UNICODE/NOMINMAX guards
 *     - ReadExact / WriteExact / ReadIpcFrame / WriteIpcFrame
 *     - privilege and executable-resolution helpers
 *
 *  2) Logging and utility primitives
 *     - EventRingBuffer
 *     - Logger
 *     - RotatingFileSink
 *     - ScopedHandle
 *
 *  3) Config and process model
 *     - ProcessState
 *     - ProgramConfig
 *     - ParseConfigResult
 *
 *  4) Shared async subsystems (scalability path)
 *     - HealthCheckScheduler (interface)
 *     - AsyncPipePump (IOCP worker pool for stdout/stderr capture)
 *     - ManagedProcess (lifecycle, pipes, restart/health behavior)
 *     - HealthCheckSchedulerImpl (shared health tick scheduler)
 *
 *  5) Supervisord core orchestration
 *     - Supervisord (config load, service control, IPC command handling)
 *
 *  6) Program entry
 *     - wmain (CLI/service mode dispatch)
 *
 * Maintenance checklist (update when extending behavior):
 *  - Add/update section anchor and index entry.
 *  - For new IPC commands: add action enum mapping + dispatch handler + help text.
 *  - For new config keys: add key handler + validation + check-config messaging.
 *  - Keep lock order policy intact (per-process command lock -> supervisor lock).
 *  - Keep IPC response codes and config issue taxonomy consistent.
 *  - Preserve snapshot-under-lock then render-outside-lock pattern for reporting.
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <winsvc.h>
#include <sddl.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <array>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <cctype>
#include <cstdlib>
#include <set>
#include <deque>
#include <unordered_set>
#include <random>
#include <functional>

#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;
static bool g_IsServiceMode = false;

namespace SupervisorDefaults {
constexpr DWORD kMaxIpcFramePayload = 64 * 1024;
constexpr size_t kDefaultLogMaxBytes = 10 * 1024 * 1024;
constexpr int kDefaultLogBackups = 5;
constexpr int kDefaultStartRetries = 3;
constexpr int kDefaultStopTimeoutSeconds = 10;
constexpr int kDefaultHealthFailures = 3;
constexpr size_t kDefaultDiagLimit = 50;
constexpr int kIpcConnectRetryCount = 5;
constexpr int kIpcBusyTimeoutMs = 2000;
constexpr int kPipeOutBuffer = 4096;
constexpr int kPipeInBuffer = 4096;
}

constexpr DWORD MAX_IPC_FRAME_PAYLOAD = SupervisorDefaults::kMaxIpcFramePayload;

namespace IpcCodes {
constexpr const char* kOkStatus = "STATUS";
constexpr const char* kOkDiag = "DIAG";
constexpr const char* kOkMetrics = "METRICS";
constexpr const char* kOkReloaded = "RELOADED";
constexpr const char* kOkStarted = "STARTED";
constexpr const char* kOkStopped = "STOPPED";
constexpr const char* kOkRestarted = "RESTARTED";

constexpr const char* kErrInvalidSyntax = "INVALID_SYNTAX";
constexpr const char* kErrAccessDenied = "ACCESS_DENIED";
constexpr const char* kErrReloadFailed = "RELOAD_FAILED";
constexpr const char* kErrMissingTarget = "MISSING_TARGET";
constexpr const char* kErrNoSuchProcess = "NO_SUCH_PROCESS";
constexpr const char* kErrUnknownCommand = "UNKNOWN_COMMAND";
constexpr const char* kErrBadFrame = "BAD_FRAME";
}

enum class IpcAction {
    Status,
    Diag,
    Metrics,
    Reload,
    Start,
    Stop,
    Restart,
    Unknown
};

struct IpcRequest {
    std::string raw;
    std::vector<std::string> tokens;
    IpcAction action = IpcAction::Unknown;
    std::string actionText;
    std::string target;
    std::wstring wTarget;
    bool clientPrivileged = false;
};

struct IpcResponse {
    bool ok = false;
    std::string code;
    std::string message;
    std::string payload;

    static IpcResponse Ok(const std::string& code, const std::string& message = std::string()) {
        IpcResponse r;
        r.ok = true;
        r.code = code;
        r.message = message;
        return r;
    }

    static IpcResponse OkWithPayload(const std::string& code, std::string payload) {
        IpcResponse r;
        r.ok = true;
        r.code = code;
        r.payload = std::move(payload);
        return r;
    }

    static IpcResponse Err(const std::string& code, const std::string& message) {
        IpcResponse r;
        r.ok = false;
        r.code = code;
        r.message = message;
        return r;
    }

    std::string ToFrame() const {
        if (ok) {
            if (!payload.empty()) return "OK|" + code + "\n" + payload;
            if (!message.empty()) return "OK|" + code + "|" + message + "\n";
            return "OK|" + code + "\n";
        }
        return "ERR|" + code + "|" + message + "\n";
    }
};

IpcAction ParseIpcAction(const std::string& action) {
    if (action == "status") return IpcAction::Status;
    if (action == "diag") return IpcAction::Diag;
    if (action == "metrics") return IpcAction::Metrics;
    if (action == "reload") return IpcAction::Reload;
    if (action == "start") return IpcAction::Start;
    if (action == "stop") return IpcAction::Stop;
    if (action == "restart") return IpcAction::Restart;
    return IpcAction::Unknown;
}

enum class ConfigIssueCode {
    UnknownKey,
    InvalidValue,
    InvalidEnvEntry,
    MissingRequiredKey,
    EmptyProgramName,
    DuplicateProgramName,
    DependencyNotFound,
    InvalidLine,
    OpenFailed
};

const char* ConfigIssueCodeText(ConfigIssueCode code) {
    switch (code) {
        case ConfigIssueCode::UnknownKey: return "CFG_UNKNOWN_KEY";
        case ConfigIssueCode::InvalidValue: return "CFG_INVALID_VALUE";
        case ConfigIssueCode::InvalidEnvEntry: return "CFG_INVALID_ENV_ENTRY";
        case ConfigIssueCode::MissingRequiredKey: return "CFG_MISSING_REQUIRED_KEY";
        case ConfigIssueCode::EmptyProgramName: return "CFG_EMPTY_PROGRAM_NAME";
        case ConfigIssueCode::DuplicateProgramName: return "CFG_DUPLICATE_PROGRAM_NAME";
        case ConfigIssueCode::DependencyNotFound: return "CFG_DEPENDENCY_NOT_FOUND";
        case ConfigIssueCode::InvalidLine: return "CFG_INVALID_LINE";
        case ConfigIssueCode::OpenFailed: return "CFG_OPEN_FAILED";
        default: return "CFG_UNKNOWN";
    }
}

void AddConfigWarning(std::vector<std::string>& warnings, int lineNo, ConfigIssueCode code, const std::string& msg) {
    std::string linePrefix = (lineNo > 0) ? ("line " + std::to_string(lineNo) + ": ") : std::string();
    warnings.push_back(linePrefix + msg + " [" + ConfigIssueCodeText(code) + "]");
}

void AddConfigError(std::vector<std::string>& errors, int lineNo, ConfigIssueCode code, const std::string& msg) {
    std::string linePrefix = (lineNo > 0) ? ("line " + std::to_string(lineNo) + ": ") : std::string();
    errors.push_back(linePrefix + msg + " [" + ConfigIssueCodeText(code) + "]");
}

struct StatusSnapshotEntry {
    std::string name;
    std::string state;
};

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

bool ReadExact(HANDLE h, char* buffer, DWORD bytesToRead) {
    DWORD total = 0;
    while (total < bytesToRead) {
        DWORD got = 0;
        if (!ReadFile(h, buffer + total, bytesToRead - total, &got, NULL) || got == 0) return false;
        total += got;
    }
    return true;
}

bool WriteExact(HANDLE h, const char* buffer, DWORD bytesToWrite) {
    DWORD total = 0;
    while (total < bytesToWrite) {
        DWORD written = 0;
        if (!WriteFile(h, buffer + total, bytesToWrite - total, &written, NULL) || written == 0) return false;
        total += written;
    }
    return true;
}

bool ReadIpcFrame(HANDLE h, std::string& payload) {
    std::string header;
    std::string preloadedPayload;
    constexpr size_t kMaxHeaderBytes = 127;
    std::array<char, 128> ioBuf{};

    while (true) {
        DWORD got = 0;
        if (!ReadFile(h, ioBuf.data(), static_cast<DWORD>(ioBuf.size()), &got, NULL) || got == 0) return false;

        const char* begin = ioBuf.data();
        const char* end = begin + got;
        const char* newline = std::find(begin, end, '\n');

        if (newline != end) {
            header.append(begin, newline);
            if (header.size() > kMaxHeaderBytes) return false;

            const char* payloadStart = newline + 1;
            if (payloadStart < end) {
                preloadedPayload.append(payloadStart, end);
            }
            break;
        }

        header.append(begin, end);
        if (header.size() > kMaxHeaderBytes) return false;
    }

    if (header.rfind("LEN:", 0) != 0) return false;
    DWORD len = 0;
    try {
        len = static_cast<DWORD>(std::stoul(header.substr(4)));
    } catch (...) {
        return false;
    }
    if (len > MAX_IPC_FRAME_PAYLOAD) return false;

    if (preloadedPayload.size() > len) return false;

    payload.resize(len);
    if (len == 0) return true;

    if (!preloadedPayload.empty()) {
        memcpy(payload.data(), preloadedPayload.data(), preloadedPayload.size());
    }

    DWORD remaining = len - static_cast<DWORD>(preloadedPayload.size());
    if (remaining == 0) return true;
    return ReadExact(h, payload.data() + preloadedPayload.size(), remaining);
}

bool WriteIpcFrame(HANDLE h, const std::string& payload) {
    if (payload.size() > MAX_IPC_FRAME_PAYLOAD) return false;
    std::string header = "LEN:" + std::to_string(payload.size()) + "\n";
    if (!WriteExact(h, header.data(), static_cast<DWORD>(header.size()))) return false;
    if (payload.empty()) return true;
    return WriteExact(h, payload.data(), static_cast<DWORD>(payload.size()));
}

bool IsTokenElevatedOrAdmin(HANDLE token) {
    TOKEN_ELEVATION elevation = {0};
    DWORD bytes = 0;
    if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &bytes) && elevation.TokenIsElevated) {
        return true;
    }

    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = NULL;
    BOOL isAdmin = FALSE;
    if (AllocateAndInitializeSid(&NtAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
        DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(token, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
}

bool IsCurrentProcessElevatedOrAdmin() {
    HANDLE token = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    bool ok = IsTokenElevatedOrAdmin(token);
    CloseHandle(token);
    return ok;
}

bool ResolveExecutablePath(const std::wstring& command, std::wstring& resolvedPath) {
    std::wstring cmd = command;
    size_t firstSpace = cmd.find_first_of(L" \t");
    std::wstring exe = (firstSpace == std::wstring::npos) ? cmd : cmd.substr(0, firstSpace);
    if (exe.empty()) return false;

    if ((exe.front() == L'"' && exe.back() == L'"') && exe.size() >= 2) {
        exe = exe.substr(1, exe.size() - 2);
    }

    wchar_t outPath[MAX_PATH] = {0};
    DWORD found = SearchPathW(NULL, exe.c_str(), L".exe", MAX_PATH, outPath, NULL);
    if (found > 0 && found < MAX_PATH) {
        resolvedPath = outPath;
        return true;
    }
    return false;
}

void WriteServiceEvent(const std::string& message, WORD type = EVENTLOG_INFORMATION_TYPE) {
    if (!g_IsServiceMode) return;

    HANDLE hEventLog = RegisterEventSourceA(NULL, "Supervisord");
    if (!hEventLog) return;

    LPCSTR strings[1] = { message.c_str() };
    ReportEventA(hEventLog, type, 0, 0x1000, NULL, 1, 0, strings, NULL);
    DeregisterEventSource(hEventLog);
}

std::vector<std::string> ParseIpcCommandTokens(const std::string& input) {
    std::vector<std::string> tokens;
    std::string current;
    bool inQuotes = false;
    bool escaping = false;

    for (char ch : input) {
        if (escaping) {
            current.push_back(ch);
            escaping = false;
            continue;
        }

        if (ch == '\\') {
            escaping = true;
            continue;
        }

        if (ch == '"') {
            inQuotes = !inQuotes;
            continue;
        }

        if (!inQuotes && std::isspace(static_cast<unsigned char>(ch))) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current.push_back(ch);
    }

    if (!current.empty()) tokens.push_back(current);
    return tokens;
}

std::string QuoteForIpc(const std::string& value) {
    if (value.find_first_of(" \t\"") == std::string::npos) return value;
    std::string out = "\"";
    for (char ch : value) {
        if (ch == '\\' || ch == '"') out.push_back('\\');
        out.push_back(ch);
    }
    out.push_back('"');
    return out;
}

// ============================================================================
// SECTION: PATH_AND_ENCODING_HELPERS
// Contract:
// - Keep path/encoding conversion isolated from process/state logic.
// - No side effects beyond deterministic conversion/lookup helpers.
// ============================================================================
std::wstring GetExecutableDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring strPath(path);
    size_t pos = strPath.find_last_of(L"\\/");
    return (pos != std::wstring::npos) ? strPath.substr(0, pos) : L".";
}

std::wstring GetUserHomeDir() {
    wchar_t* userProfile = nullptr;
    size_t len = 0;
    if (_wdupenv_s(&userProfile, &len, L"USERPROFILE") == 0 && userProfile != nullptr) {
        std::wstring home(userProfile);
        free(userProfile);
        return home;
    }
    wchar_t* homeDrive = nullptr;
    wchar_t* homePath = nullptr;
    size_t lenDrive = 0, lenPath = 0;
    _wdupenv_s(&homeDrive, &lenDrive, L"HOMEDRIVE");
    _wdupenv_s(&homePath, &lenPath, L"HOMEPATH");
    if (homeDrive && homePath) {
        std::wstring home = std::wstring(homeDrive) + std::wstring(homePath);
        free(homeDrive);
        free(homePath);
        return home;
    }
    if (homeDrive) free(homeDrive);
    if (homePath) free(homePath);
    return GetExecutableDir();
}

std::string WideToUtf8(const std::wstring& input) {
    if (input.empty()) return std::string();

    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, NULL, 0, NULL, NULL);
    if (sizeNeeded <= 1) return std::string();

    std::string result(static_cast<size_t>(sizeNeeded - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, result.data(), sizeNeeded, NULL, NULL);
    return result;
}

std::wstring Utf8ToWide(const std::string& input) {
    if (input.empty()) return std::wstring();

    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, NULL, 0);
    if (sizeNeeded <= 1) return std::wstring(input.begin(), input.end());

    std::wstring result(static_cast<size_t>(sizeNeeded - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, result.data(), sizeNeeded);
    return result;
}

// ============================================================================
// SECTION: LOGGING_AND_ROTATION
// Contract:
// - Logging must not throw across call boundaries.
// - Rotation errors are counted and reported, not fatal to supervision.
// ============================================================================
class EventRingBuffer {
    static inline std::mutex g_evtMtx;
    static inline std::deque<std::string> g_events;
    static inline const size_t g_capacity = 200;
public:
    static void Add(const std::string& line) {
        std::lock_guard<std::mutex> lock(g_evtMtx);
        if (g_events.size() >= g_capacity) g_events.pop_front();
        g_events.push_back(line);
    }

    static std::string Dump(size_t limit = 50) {
        std::lock_guard<std::mutex> lock(g_evtMtx);
        if (g_events.empty()) return "<no events>\n";
        size_t count = std::min(limit, g_events.size());
        size_t start = g_events.size() - count;
        std::stringstream ss;
        for (size_t i = start; i < g_events.size(); ++i) ss << g_events[i] << "\n";
        return ss.str();
    }
};

class Logger {
    static inline std::mutex g_logMtx;
public:
    static void Log(const std::string& prefix, const std::string& message) {
        std::lock_guard<std::mutex> lock(g_logMtx);
        auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::tm tm;
        localtime_s(&tm, &now);
        std::stringstream line;
        line << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S] ") << "[" << prefix << "] " << message;
        std::cout << line.str() << std::endl;
        EventRingBuffer::Add(line.str());
        WriteServiceEvent("[" + prefix + "] " + message, EVENTLOG_INFORMATION_TYPE);
    }
};

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
    RotatingFileSink(std::wstring path, size_t maxBytes = SupervisorDefaults::kDefaultLogMaxBytes, int maxBackups = SupervisorDefaults::kDefaultLogBackups)
        : filePath_(std::move(path)), maxBytes_(maxBytes), maxBackups_(maxBackups) {
        OpenFileIfNeeded();
    }

    ~RotatingFileSink() {
        CloseFile();
    }

    static void ReportStats() {
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

    void Write(const char* data, DWORD size) {
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

private:
    void CloseFile() {
        if (hFile_ != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile_);
            hFile_ = INVALID_HANDLE_VALUE;
        }
    }

    void OpenFileIfNeeded() {
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

    void RotateIfNeeded(DWORD incomingBytes) {
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
};

// ============================================================================
// SECTION: RAII_AND_CONFIG_MODEL
// Contract:
// - Keep trivial ownership/data definitions here.
// - Do not embed runtime orchestration behavior in config model structs.
// ============================================================================
class ScopedHandle {
    HANDLE h_ = INVALID_HANDLE_VALUE;
public:
    ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) : h_(h) {}
    ~ScopedHandle() { Close(); }
    void Close() {
        if (h_ != INVALID_HANDLE_VALUE && h_ != NULL) {
            CloseHandle(h_);
            h_ = INVALID_HANDLE_VALUE;
        }
    }
    HANDLE get() const { return h_; }
    HANDLE* replace() { Close(); return &h_; }
    bool isValid() const { return h_ != INVALID_HANDLE_VALUE && h_ != NULL; }
    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;
    ScopedHandle(ScopedHandle&& o) noexcept : h_(o.h_) { o.h_ = INVALID_HANDLE_VALUE; }
    ScopedHandle& operator=(ScopedHandle&& o) noexcept {
        if (this != &o) { Close(); h_ = o.h_; o.h_ = INVALID_HANDLE_VALUE; }
        return *this;
    }
};

enum class ProcessState { STOPPED, STARTING, RUNNING, BACKOFF, STOPPING, FATAL };

std::string StateToString(ProcessState state) {
    switch (state) {
        case ProcessState::STOPPED:  return "STOPPED";
        case ProcessState::STARTING: return "STARTING";
        case ProcessState::RUNNING:  return "RUNNING";
        case ProcessState::BACKOFF:  return "BACKOFF";
        case ProcessState::STOPPING: return "STOPPING";
        case ProcessState::FATAL:    return "FATAL";
        default:                     return "UNKNOWN";
    }
}

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

struct ParseConfigResult {
    std::vector<ProgramConfig> configs;
    std::vector<std::string> warnings;
    std::vector<std::string> errors;
};

// ============================================================================
// SECTION: CONFIG_PARSE_VALIDATE
// Contract:
// - Parse phase should only read/normalize tokens into ProgramConfig.
// - Validation phase should emit diagnostics without mutating runtime state.
// ============================================================================
std::vector<wchar_t> CreateEnvironmentBlock(const std::map<std::wstring, std::wstring>& customEnv) {
    LPWCH sysEnv = GetEnvironmentStringsW();
    std::map<std::wstring, std::wstring> envMap;

    LPWCH var = sysEnv;
    while (*var) {
        std::wstring s(var);
        size_t eq = s.find(L'=');
        if (eq != std::wstring::npos && eq > 0) {
            envMap[s.substr(0, eq)] = s.substr(eq + 1);
        }
        var += s.length() + 1;
    }
    FreeEnvironmentStringsW(sysEnv);

    for (const auto& [k, v] : customEnv) {
        envMap[k] = v;
    }

    std::vector<wchar_t> block;
    for (const auto& [k, v] : envMap) {
        std::wstring entry = k + L"=" + v;
        block.insert(block.end(), entry.begin(), entry.end());
        block.push_back(L'\0');
    }
    block.push_back(L'\0');
    return block;
}

bool TryParseInt(const std::wstring& input, int& out) {
    if (input.empty()) return false;
    try {
        size_t idx = 0;
        int value = std::stoi(input, &idx);
        if (idx != input.size()) return false;
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool TryParseUnsignedLongLong(const std::wstring& input, unsigned long long& out) {
    if (input.empty()) return false;
    try {
        size_t idx = 0;
        unsigned long long value = std::stoull(input, &idx);
        if (idx != input.size()) return false;
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool TryParseDword(const std::wstring& input, DWORD& out) {
    unsigned long long value = 0;
    if (!TryParseUnsignedLongLong(input, value) || value > 0xFFFFFFFFull) return false;
    out = static_cast<DWORD>(value);
    return true;
}

std::wstring TrimConfigToken(std::wstring s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](wchar_t ch) { return !std::isspace(ch); }));
    s.erase(std::find_if(s.rbegin(), s.rend(), [](wchar_t ch) { return !std::isspace(ch); }).base(), s.end());
    if (s.size() >= 2 && s.front() == L'"' && s.back() == L'"') {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

using ConfigKeyHandler = std::function<void(ProgramConfig&, const std::wstring&, int, ParseConfigResult&)>;

std::map<std::wstring, ConfigKeyHandler> BuildProgramKeyHandlers() {
    std::map<std::wstring, ConfigKeyHandler> handlers;

    handlers[L"command"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.command = val;
    };

    handlers[L"directory"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.directory = val;
    };

    handlers[L"depends_on"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.depends_on.clear();
        std::wstringstream ss(val);
        std::wstring item;
        while (std::getline(ss, item, L',')) {
            std::wstring dep = TrimConfigToken(item);
            if (!dep.empty()) cfg.depends_on.push_back(dep);
        }
    };

    handlers[L"shutdown_phase"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        int parsed = 0;
        if (TryParseInt(val, parsed)) cfg.shutdown_phase = parsed;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid shutdown_phase value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"autostart"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        if (val == L"true" || val == L"1") cfg.autostart = true;
        else if (val == L"false" || val == L"0") cfg.autostart = false;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid autostart value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"autorestart"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.autorestart = val;
    };

    handlers[L"startretries"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        int parsed = 0;
        if (TryParseInt(val, parsed)) cfg.startretries = parsed;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid startretries value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"stoptimeout"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        int parsed = 0;
        if (TryParseInt(val, parsed)) cfg.stoptimeout = parsed;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid stoptimeout value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"stop_command"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.stop_command = val;
    };

    handlers[L"healthcheck_command"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.healthcheck_command = val;
    };

    handlers[L"healthcheck_interval"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        int parsed = 0;
        if (TryParseInt(val, parsed)) cfg.healthcheck_interval = parsed;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid healthcheck_interval value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"healthcheck_failures"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        int parsed = 0;
        if (TryParseInt(val, parsed)) cfg.healthcheck_failures = parsed;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid healthcheck_failures value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"stdout_logfile"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.stdout_logfile = val;
    };

    handlers[L"stdout_logfile_maxbytes"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        unsigned long long parsed = 0;
        if (TryParseUnsignedLongLong(val, parsed)) cfg.stdout_logfile_maxbytes = static_cast<size_t>(parsed);
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid stdout_logfile_maxbytes value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"stdout_logfile_backups"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        int parsed = 0;
        if (TryParseInt(val, parsed)) cfg.stdout_logfile_backups = parsed;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid stdout_logfile_backups value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"stderr_logfile"] = [](ProgramConfig& cfg, const std::wstring& val, int, ParseConfigResult&) {
        cfg.stderr_logfile = val;
    };

    handlers[L"stderr_logfile_maxbytes"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        unsigned long long parsed = 0;
        if (TryParseUnsignedLongLong(val, parsed)) cfg.stderr_logfile_maxbytes = static_cast<size_t>(parsed);
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid stderr_logfile_maxbytes value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"stderr_logfile_backups"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        int parsed = 0;
        if (TryParseInt(val, parsed)) cfg.stderr_logfile_backups = parsed;
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid stderr_logfile_backups value '" + WideToUtf8(val) + "' (keeping default)");
    };

    handlers[L"environment"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        std::wstringstream ss(val);
        std::wstring item;
        while (std::getline(ss, item, L',')) {
            size_t kPos = item.find(L'=');
            if (kPos != std::wstring::npos) {
                cfg.environment[TrimConfigToken(item.substr(0, kPos))] = TrimConfigToken(item.substr(kPos + 1));
            } else {
                AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidEnvEntry, "invalid environment entry '" + WideToUtf8(item) + "'");
            }
        }
    };

    handlers[L"exitcodes"] = [](ProgramConfig& cfg, const std::wstring& val, int lineNo, ParseConfigResult& result) {
        std::vector<DWORD> parsedExitCodes;
        std::wstringstream ss(val);
        std::wstring token;
        while (std::getline(ss, token, L',')) {
            DWORD parsed = 0;
            std::wstring t = TrimConfigToken(token);
            if (TryParseDword(t, parsed)) {
                parsedExitCodes.push_back(parsed);
            } else {
                AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "invalid exit code token '" + WideToUtf8(t) + "'");
            }
        }
        if (!parsedExitCodes.empty()) cfg.exitcodes = std::move(parsedExitCodes);
        else AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidValue, "exitcodes list was empty or invalid (keeping default)");
    };

    return handlers;
}

void ValidateParsedPrograms(ParseConfigResult& result) {
    std::unordered_set<std::wstring> names;
    for (const auto& cfg : result.configs) {
        if (!names.insert(cfg.name).second) {
            AddConfigError(result.errors, 0, ConfigIssueCode::DuplicateProgramName, "duplicate program name: " + WideToUtf8(cfg.name));
        }
    }

    std::unordered_set<std::wstring> knownNames;
    for (const auto& cfg : result.configs) knownNames.insert(cfg.name);

    for (const auto& cfg : result.configs) {
        if (!cfg.directory.empty() && !fs::exists(cfg.directory)) {
            AddConfigWarning(result.warnings, 0, ConfigIssueCode::InvalidValue, "program '" + WideToUtf8(cfg.name) + "': directory does not exist: " + WideToUtf8(cfg.directory));
        }

        std::wstring resolved;
        if (!ResolveExecutablePath(cfg.command, resolved)) {
            AddConfigWarning(result.warnings, 0, ConfigIssueCode::InvalidValue, "program '" + WideToUtf8(cfg.name) + "': command may not resolve on PATH: " + WideToUtf8(cfg.command));
        }

        if (!cfg.stdout_logfile.empty() && !cfg.stderr_logfile.empty() && cfg.stdout_logfile == cfg.stderr_logfile) {
            AddConfigWarning(result.warnings, 0, ConfigIssueCode::InvalidValue, "program '" + WideToUtf8(cfg.name) + "': stdout_logfile and stderr_logfile are the same path");
        }

        if (cfg.stoptimeout <= 0) {
            AddConfigWarning(result.warnings, 0, ConfigIssueCode::InvalidValue, "program '" + WideToUtf8(cfg.name) + "': non-positive stoptimeout may force immediate termination");
        }

        if (!cfg.healthcheck_command.empty() && cfg.healthcheck_interval <= 0) {
            AddConfigWarning(result.warnings, 0, ConfigIssueCode::InvalidValue, "program '" + WideToUtf8(cfg.name) + "': healthcheck_command set but healthcheck_interval <= 0, probe disabled");
        }

        for (const auto& dep : cfg.depends_on) {
            if (knownNames.count(dep) == 0) {
                AddConfigWarning(result.warnings, 0, ConfigIssueCode::DependencyNotFound, "program '" + WideToUtf8(cfg.name) + "': dependency not found: " + WideToUtf8(dep));
            }
        }
    }
}

ParseConfigResult ParseConfigDetailed(const std::wstring& filepath) {
    ParseConfigResult result;
    std::wifstream file(filepath);
    if (!file.is_open()) {
        AddConfigError(result.errors, 0, ConfigIssueCode::OpenFailed, "Could not open config file: " + WideToUtf8(filepath));
        return result;
    }

    std::wstring line, currentSection;
    static const std::map<std::wstring, ConfigKeyHandler> keyHandlers = BuildProgramKeyHandlers();
    ProgramConfig currentConfig;
    bool inProgramSection = false;
    int currentSectionStartLine = 0;
    int lineNo = 0;

    auto finalizeSection = [&]() {
        if (!inProgramSection) return;
        if (currentConfig.name.empty()) {
            AddConfigError(result.errors, currentSectionStartLine, ConfigIssueCode::EmptyProgramName, "program section has empty name");
        } else if (currentConfig.command.empty()) {
            AddConfigError(result.errors, currentSectionStartLine, ConfigIssueCode::MissingRequiredKey, "program '" + WideToUtf8(currentConfig.name) + "' is missing required key 'command'");
        } else {
            result.configs.push_back(currentConfig);
        }
        currentConfig = ProgramConfig();
    };

    while (std::getline(file, line)) {
        lineNo++;
        size_t commentPos = line.find_first_of(L";#");
        if (commentPos != std::wstring::npos) line = line.substr(0, commentPos);
        line = TrimConfigToken(line);
        if (line.empty()) continue;

        if (line.front() == L'[' && line.back() == L']') {
            finalizeSection();
            currentSection = line.substr(1, line.size() - 2);
            if (currentSection.rfind(L"program:", 0) == 0) {
                inProgramSection = true;
                currentConfig.name = currentSection.substr(8);
                currentSectionStartLine = lineNo;
                if (currentConfig.name.empty()) {
                    AddConfigError(result.errors, lineNo, ConfigIssueCode::EmptyProgramName, "program section name cannot be empty");
                }
            } else {
                inProgramSection = false;
            }
            continue;
        }

        if (!inProgramSection) continue;

        size_t eqPos = line.find(L'=');
        if (eqPos != std::wstring::npos) {
            std::wstring key = TrimConfigToken(line.substr(0, eqPos));
            std::wstring val = TrimConfigToken(line.substr(eqPos + 1));
            auto it = keyHandlers.find(key);
            if (it != keyHandlers.end()) {
                it->second(currentConfig, val, lineNo, result);
            } else {
                AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::UnknownKey, "unknown key '" + WideToUtf8(key) + "' ignored");
            }
        } else {
            AddConfigWarning(result.warnings, lineNo, ConfigIssueCode::InvalidLine, "ignored non key=value line");
        }
    }
    finalizeSection();

    ValidateParsedPrograms(result);

    return result;
}

// ============================================================================
// SECTION: MANAGED_PROCESS_RUNTIME
// Contract:
// - Own process lifecycle and restart/health behavior for one program.
// - No direct supervisor map mutation from this class.
// ============================================================================
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
class AsyncPipePump {
public:
    static AsyncPipePump& Instance() {
        static AsyncPipePump pump;
        return pump;
    }

    bool RegisterPipe(HANDLE sourceReadHandle, const std::wstring& path, size_t maxBytes, int backups) {
        if (sourceReadHandle == NULL || sourceReadHandle == INVALID_HANDLE_VALUE) return false;

        HANDLE asyncReadHandle = INVALID_HANDLE_VALUE;
        if (!DuplicateHandle(GetCurrentProcess(), sourceReadHandle, GetCurrentProcess(), &asyncReadHandle, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
            return false;
        }

        if (CreateIoCompletionPort(asyncReadHandle, iocp_, 0, 0) == NULL) {
            CloseHandle(asyncReadHandle);
            return false;
        }

        auto ctx = std::make_shared<PipeContext>();
        ctx->sourceKey = reinterpret_cast<uintptr_t>(sourceReadHandle);
        ctx->readHandle = asyncReadHandle;
        ctx->sink = std::make_unique<RotatingFileSink>(path, maxBytes, backups);

        {
            std::lock_guard<std::mutex> lock(mtx_);
            bySource_[ctx->sourceKey] = ctx;
            byOverlapped_[&ctx->ov] = ctx;
        }

        if (!IssueRead(ctx)) {
            RetireContext(ctx);
            return false;
        }
        return true;
    }

    void UnregisterPipe(HANDLE sourceReadHandle) {
        if (sourceReadHandle == NULL || sourceReadHandle == INVALID_HANDLE_VALUE) return;
        std::shared_ptr<PipeContext> ctx;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            auto it = bySource_.find(reinterpret_cast<uintptr_t>(sourceReadHandle));
            if (it == bySource_.end()) return;
            ctx = it->second;
        }

        ctx->stopping = true;
        CancelIoEx(ctx->readHandle, &ctx->ov);
        PostQueuedCompletionStatus(iocp_, 0, 0, &ctx->ov);

        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [&]() { return ctx->retired.load(); });
    }

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

    AsyncPipePump() {
        iocp_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
        size_t hw = static_cast<size_t>(std::max(1u, std::thread::hardware_concurrency()));
        size_t workerCount = std::max<size_t>(2, std::min<size_t>(8, hw));
        workers_.reserve(workerCount);
        for (size_t i = 0; i < workerCount; ++i) {
            workers_.emplace_back([this]() { WorkerLoop(); });
        }
    }

    ~AsyncPipePump() {
        if (iocp_) {
            for (size_t i = 0; i < workers_.size(); ++i) {
                PostQueuedCompletionStatus(iocp_, 0, 1, NULL);
            }
            for (auto& t : workers_) {
                if (t.joinable()) t.join();
            }
            CloseHandle(iocp_);
            iocp_ = NULL;
        }
    }

    bool IssueRead(const std::shared_ptr<PipeContext>& ctx) {
        if (ctx->stopping.load()) return false;
        ZeroMemory(&ctx->ov, sizeof(ctx->ov));
        BOOL ok = ReadFile(ctx->readHandle, ctx->buffer.data(), static_cast<DWORD>(ctx->buffer.size()), NULL, &ctx->ov);
        if (ok) return true;
        DWORD err = GetLastError();
        return err == ERROR_IO_PENDING;
    }

    void WorkerLoop() {
        while (true) {
            DWORD bytes = 0;
            ULONG_PTR key = 0;
            LPOVERLAPPED ov = NULL;
            BOOL ok = GetQueuedCompletionStatus(iocp_, &bytes, &key, &ov, INFINITE);

            if (ov == NULL) {
                if (key == 1) break;
                continue;
            }

            std::shared_ptr<PipeContext> ctx;
            {
                std::lock_guard<std::mutex> lock(mtx_);
                auto it = byOverlapped_.find(ov);
                if (it == byOverlapped_.end()) continue;
                ctx = it->second;
            }

            if (ctx->stopping.load()) {
                RetireContext(ctx);
                continue;
            }

            if (!ok) {
                DWORD err = GetLastError();
                if (err == ERROR_BROKEN_PIPE || err == ERROR_PIPE_NOT_CONNECTED || err == ERROR_OPERATION_ABORTED) {
                    RetireContext(ctx);
                    continue;
                }
                RetireContext(ctx);
                continue;
            }

            if (bytes > 0) {
                ctx->sink->Write(ctx->buffer.data(), bytes);
            }

            if (!IssueRead(ctx)) {
                RetireContext(ctx);
            }
        }
    }

    void RetireContext(const std::shared_ptr<PipeContext>& ctx) {
        if (ctx->retired.exchange(true)) return;

        {
            std::lock_guard<std::mutex> lock(mtx_);
            byOverlapped_.erase(&ctx->ov);
            bySource_.erase(ctx->sourceKey);
        }

        if (ctx->readHandle != INVALID_HANDLE_VALUE) {
            CloseHandle(ctx->readHandle);
            ctx->readHandle = INVALID_HANDLE_VALUE;
        }

        cv_.notify_all();
    }
};

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

    ManagedProcess(const ProgramConfig& cfg) : config(cfg), state_(ProcessState::STOPPED) {}

    ~ManagedProcess() { Stop(); }

    void Start() {
        std::lock_guard<std::mutex> lifecycleLock(lifecycle_mtx_);
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (state_ == ProcessState::RUNNING || state_ == ProcessState::STARTING) return;
            should_run_ = true;
            retry_count_ = 0;
        }
        JoinWorkerThread();
        worker_thread_ = std::thread(&ManagedProcess::RunLoop, this);
    }

    void Stop() {
        std::lock_guard<std::mutex> lifecycleLock(lifecycle_mtx_);
        should_run_ = false;
        StopHealthMonitor();
        bool shouldKill = false;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (state_ == ProcessState::RUNNING || state_ == ProcessState::STARTING) {
                state_ = ProcessState::STOPPING;
                shouldKill = true;
            }
        }
        if (shouldKill) KillProcessTree();
        JoinWorkerThread();
        StopPipeLogging();
        hStdOutRead_.Close();
        hStdErrRead_.Close();
        state_ = ProcessState::STOPPED;
    }

    ProcessState GetState() const { return state_; }
    std::wstring GetName() const { return config.name; }

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

    void JoinWorkerThread() {
        if (worker_thread_.joinable() && worker_thread_.get_id() != std::this_thread::get_id()) {
            worker_thread_.join();
        }
    }

    void RequestHealthStop() {
        stop_health_ = true;
    }

    void StartPipeLogging(HANDLE hReadPipe, const std::wstring& path, size_t maxBytes, int backups) {
        if (hReadPipe == NULL || hReadPipe == INVALID_HANDLE_VALUE) return;
        if (!AsyncPipePump::Instance().RegisterPipe(hReadPipe, path, maxBytes, backups)) {
            Log("Failed to register async log pipe.");
        }
    }

    void StopPipeLogging() {
        if (hStdOutRead_.isValid()) {
            AsyncPipePump::Instance().UnregisterPipe(hStdOutRead_.get());
        }
        if (hStdErrRead_.isValid()) {
            AsyncPipePump::Instance().UnregisterPipe(hStdErrRead_.get());
        }
    }

    void StopHealthMonitor() {
        RequestHealthStop();
        if (health_registered_.exchange(false)) {
            HealthCheckScheduler::Instance().UnregisterProcess(this);
        }
    }

    void Log(const std::string& msg) {
        std::string nameStr = WideToUtf8(config.name);
        Logger::Log(nameStr, msg);
    }

    void CreateJob() {
        hJob_ = CreateJobObjectW(NULL, NULL);
        if (hJob_.isValid()) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(hJob_.get(), JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        }
    }

    bool RunBestEffortStopCommand() {
        if (config.stop_command.empty()) return false;

        std::vector<wchar_t> cmdBuffer(config.stop_command.begin(), config.stop_command.end());
        cmdBuffer.push_back(L'\0');

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi = {0};
        LPCWSTR pDir = config.directory.empty() ? NULL : config.directory.c_str();

        BOOL ok = CreateProcessW(
            NULL,
            cmdBuffer.data(),
            NULL,
            NULL,
            FALSE,
            CREATE_NO_WINDOW,
            NULL,
            pDir,
            &si,
            &pi
        );
        if (!ok) {
            Log("stop_command failed to launch. Win32 Error: " + std::to_string(GetLastError()));
            return false;
        }

        ScopedHandle hProc(pi.hProcess);
        ScopedHandle hThread(pi.hThread);
        WaitForSingleObject(hProc.get(), 2000);
        Log("stop_command executed before shutdown signal.");
        return true;
    }

    int ComputeBackoffMs(int attempt) {
        int exp = std::min(attempt, 6);
        int baseMs = 500 * (1 << exp);
        baseMs = std::min(baseMs, 30000);
        static thread_local std::mt19937 rng(static_cast<unsigned int>(GetTickCount64()));
        std::uniform_int_distribution<int> jitter(0, 500);
        return baseMs + jitter(rng);
    }

    bool RunHealthCheckOnce() {
        if (config.healthcheck_command.empty()) return true;

        std::vector<wchar_t> cmdBuffer(config.healthcheck_command.begin(), config.healthcheck_command.end());
        cmdBuffer.push_back(L'\0');
        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        PROCESS_INFORMATION pi = {0};
        LPCWSTR pDir = config.directory.empty() ? NULL : config.directory.c_str();

        BOOL ok = CreateProcessW(NULL, cmdBuffer.data(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, pDir, &si, &pi);
        if (!ok) return false;

        ScopedHandle hProc(pi.hProcess);
        ScopedHandle hThread(pi.hThread);
        WaitForSingleObject(hProc.get(), 5000);
        DWORD exitCode = 1;
        GetExitCodeProcess(hProc.get(), &exitCode);
        return exitCode == 0;
    }

    void StartHealthMonitor() {
        stop_health_ = false;
        if (config.healthcheck_command.empty() || config.healthcheck_interval <= 0) return;
        health_failures_ = 0;
        if (!health_registered_.exchange(true)) {
            HealthCheckScheduler::Instance().RegisterProcess(this, config.healthcheck_interval);
        }
    }

public:
    void OnSharedHealthCheckTick() {
        if (stop_health_.load() || !should_run_.load()) return;

        if (RunHealthCheckOnce()) {
            health_failures_ = 0;
            return;
        }

        int consecutiveFailures = ++health_failures_;
        g_totalHealthCheckFailures++;
        Log("Health check failed (" + std::to_string(consecutiveFailures) + ")");

        if (consecutiveFailures >= std::max(1, config.healthcheck_failures)) {
            Log("Health check threshold reached. Terminating process for restart policy.");
            if (hProcess_.isValid()) {
                TerminateProcess(hProcess_.get(), 87);
            }
        }
    }

private:

    bool Launch() {
        CreateJob();

        SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
        HANDLE hStdOutRead = NULL, hStdOutWrite = NULL;
        HANDLE hStdErrRead = NULL, hStdErrWrite = NULL;

        if (!config.stdout_logfile.empty()) {
            CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0);
            SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
            hStdOutRead_ = hStdOutRead;
            StartPipeLogging(hStdOutRead_.get(), config.stdout_logfile, config.stdout_logfile_maxbytes, config.stdout_logfile_backups);
        }

        if (!config.stderr_logfile.empty()) {
            CreatePipe(&hStdErrRead, &hStdErrWrite, &sa, 0);
            SetHandleInformation(hStdErrRead, HANDLE_FLAG_INHERIT, 0);
            hStdErrRead_ = hStdErrRead;
            StartPipeLogging(hStdErrRead_.get(), config.stderr_logfile, config.stderr_logfile_maxbytes, config.stderr_logfile_backups);
        }

        STARTUPINFOW si = { sizeof(STARTUPINFOW) };
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hStdOutWrite ? hStdOutWrite : GetStdHandle(STD_OUTPUT_HANDLE);
        si.hStdError  = hStdErrWrite ? hStdErrWrite : GetStdHandle(STD_ERROR_HANDLE);
        si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

        PROCESS_INFORMATION pi = { 0 };
        std::vector<wchar_t> cmdBuffer(config.command.begin(), config.command.end());
        cmdBuffer.push_back(L'\0');

        LPCWSTR pDir = config.directory.empty() ? NULL : config.directory.c_str();
        auto envBlock = CreateEnvironmentBlock(config.environment);

        DWORD creationFlags = CREATE_NEW_PROCESS_GROUP | CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT;

        BOOL success = CreateProcessW(
            NULL, cmdBuffer.data(), NULL, NULL, TRUE,
            creationFlags, envBlock.data(), pDir, &si, &pi
        );

        if (hStdOutWrite) CloseHandle(hStdOutWrite);
        if (hStdErrWrite) CloseHandle(hStdErrWrite);

        if (!success) {
            g_totalLaunchFailures++;
            Log("Failed to launch process. Win32 Error: " + std::to_string(GetLastError()));
            StopPipeLogging();
            hStdOutRead_.Close();
            hStdErrRead_.Close();
            return false;
        }

        g_totalStarts++;

        hProcess_ = pi.hProcess;
        ScopedHandle hThread(pi.hThread);

        if (hJob_.isValid()) AssignProcessToJobObject(hJob_.get(), hProcess_.get());
        ResumeThread(hThread.get());
        return true;
    }

    void KillProcessTree() {
        if (!hProcess_.isValid()) return;
        DWORD pid = GetProcessId(hProcess_.get());

        RunBestEffortStopCommand();

        if (!GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid)) {
            // Only attempt to attach when we are not already bound to a console.
            if (GetConsoleWindow() == NULL) {
                if (AttachConsole(pid)) {
                    GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, pid);
                    FreeConsole();
                }
            }
        }

        if (WaitForSingleObject(hProcess_.get(), config.stoptimeout * 1000) == WAIT_TIMEOUT) {
            Log("Timeout reached. Force terminating process tree...");
            TerminateProcess(hProcess_.get(), 1);
        }

        if (hJob_.isValid()) hJob_.Close();
        hProcess_.Close();

        StopHealthMonitor();
        StopPipeLogging();

        // Close read pipes to unblock ReadFile if grandchild process inherited write pipe
        hStdOutRead_.Close();
        hStdErrRead_.Close();
    }

    void RunLoop() {
        while (should_run_) {
            state_ = (retry_count_ == 0) ? ProcessState::STARTING : ProcessState::BACKOFF;
            Log(state_ == ProcessState::STARTING ? "Starting process..." : "Restarting (Attempt " + std::to_string(retry_count_) + ")...");

            if (!Launch()) {
                retry_count_++;
                if (retry_count_ > config.startretries) {
                    state_ = ProcessState::FATAL;
                    Log("Process entered FATAL state.");
                    should_run_ = false;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(ComputeBackoffMs(retry_count_)));
                continue;
            }

            state_ = ProcessState::RUNNING;
            Log("Process running cleanly.");
            StartHealthMonitor();

            WaitForSingleObject(hProcess_.get(), INFINITE);

            DWORD exitCode = 0;
            GetExitCodeProcess(hProcess_.get(), &exitCode);
            hProcess_.Close();

            // Explicitly close read pipes so logging threads exit cleanly
            StopPipeLogging();
            hStdOutRead_.Close();
            hStdErrRead_.Close();
            StopHealthMonitor();

            if (!should_run_) {
                state_ = ProcessState::STOPPED;
                Log("Process stopped by user request.");
                break;
            }

            Log("Process terminated with exit code " + std::to_string(exitCode));
            g_totalUnexpectedExits++;

            bool isExpected = std::find(config.exitcodes.begin(), config.exitcodes.end(), exitCode) != config.exitcodes.end();
            bool shouldRestart = (config.autorestart == L"true" || config.autorestart == L"1") ||
                                 (config.autorestart == L"unexpected" && !isExpected);

            if (shouldRestart) {
                retry_count_++;
                g_totalRestarts++;
                if (retry_count_ > config.startretries) {
                    state_ = ProcessState::FATAL;
                    Log("Exceeded max startretries. Entering FATAL state.");
                    should_run_ = false;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(ComputeBackoffMs(retry_count_)));
            } else {
                state_ = ProcessState::STOPPED;
                should_run_ = false;
                break;
            }
        }
    }
};

namespace {
// HealthCheckSchedulerImpl public surface:
// - RegisterProcess/UnregisterProcess are safe under concurrent lifecycle operations.
// - Unregister waits until in-flight callback for that process has completed.
// - One shared worker thread schedules all health checks.
class HealthCheckSchedulerImpl {
public:
    HealthCheckSchedulerImpl() {
        worker_ = std::thread([this]() { Run(); });
    }

    ~HealthCheckSchedulerImpl() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stop_ = true;
            cv_.notify_all();
        }
        if (worker_.joinable()) worker_.join();
    }

    void RegisterProcess(ManagedProcess* process, int intervalSeconds) {
        if (process == nullptr || intervalSeconds <= 0) return;
        std::lock_guard<std::mutex> lock(mtx_);
        Entry e;
        e.intervalSeconds = intervalSeconds;
        e.nextDue = std::chrono::steady_clock::now() + std::chrono::seconds(intervalSeconds);
        entries_[process] = e;
        cv_.notify_all();
    }

    void UnregisterProcess(ManagedProcess* process) {
        if (process == nullptr) return;
        std::unique_lock<std::mutex> lock(mtx_);
        entries_.erase(process);
        cv_.notify_all();
        cv_.wait(lock, [&]() { return inFlight_.count(process) == 0; });
    }

private:
    struct Entry {
        int intervalSeconds = 0;
        std::chrono::steady_clock::time_point nextDue;
    };

    std::mutex mtx_;
    std::condition_variable cv_;
    bool stop_ = false;
    std::thread worker_;
    std::map<ManagedProcess*, Entry> entries_;
    std::set<ManagedProcess*> inFlight_;

    void Run() {
        std::unique_lock<std::mutex> lock(mtx_);
        while (!stop_) {
            if (entries_.empty()) {
                cv_.wait(lock, [&]() { return stop_ || !entries_.empty(); });
                continue;
            }

            auto earliest = std::min_element(
                entries_.begin(),
                entries_.end(),
                [](const auto& a, const auto& b) { return a.second.nextDue < b.second.nextDue; }
            );

            auto nextDue = earliest->second.nextDue;
            cv_.wait_until(lock, nextDue, [&]() { return stop_; });
            if (stop_) break;

            auto now = std::chrono::steady_clock::now();
            std::vector<ManagedProcess*> due;
            for (auto& [proc, entry] : entries_) {
                if (entry.nextDue <= now) {
                    due.push_back(proc);
                    entry.nextDue = now + std::chrono::seconds(entry.intervalSeconds);
                    inFlight_.insert(proc);
                }
            }

            for (ManagedProcess* proc : due) {
                lock.unlock();
                proc->OnSharedHealthCheckTick();
                lock.lock();
                inFlight_.erase(proc);
                cv_.notify_all();
            }
        }
    }
};
} // namespace

HealthCheckScheduler& HealthCheckScheduler::Instance() {
    static HealthCheckScheduler instance;
    return instance;
}

static HealthCheckSchedulerImpl& GetHealthCheckSchedulerImpl() {
    static HealthCheckSchedulerImpl impl;
    return impl;
}

void HealthCheckScheduler::RegisterProcess(ManagedProcess* process, int intervalSeconds) {
    GetHealthCheckSchedulerImpl().RegisterProcess(process, intervalSeconds);
}

void HealthCheckScheduler::UnregisterProcess(ManagedProcess* process) {
    GetHealthCheckSchedulerImpl().UnregisterProcess(process);
}

// ============================================================================
// SECTION: IPC_SERVER_AND_CLIENT
// Contract:
// - Keep framing and response protocol stable: OK|CODE[...] / ERR|CODE|MESSAGE.
// - Keep IPC handlers side-effect scoped and privilege-checked per command.
// ============================================================================
const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\supervisord";

bool IsPipeClientPrivileged(HANDLE hPipe) {
    if (!ImpersonateNamedPipeClient(hPipe)) return false;

    HANDLE token = NULL;
    bool allowed = false;
    if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &token)) {
        allowed = IsTokenElevatedOrAdmin(token);
        CloseHandle(token);
    }
    RevertToSelf();
    return allowed;
}

bool ProgramConfigEquals(const ProgramConfig& a, const ProgramConfig& b) {
    return a.name == b.name &&
           a.command == b.command &&
           a.directory == b.directory &&
           a.depends_on == b.depends_on &&
           a.shutdown_phase == b.shutdown_phase &&
           a.autostart == b.autostart &&
           a.autorestart == b.autorestart &&
           a.exitcodes == b.exitcodes &&
           a.startretries == b.startretries &&
           a.stoptimeout == b.stoptimeout &&
           a.stop_command == b.stop_command &&
           a.healthcheck_command == b.healthcheck_command &&
           a.healthcheck_interval == b.healthcheck_interval &&
           a.healthcheck_failures == b.healthcheck_failures &&
           a.stdout_logfile == b.stdout_logfile &&
           a.stdout_logfile_maxbytes == b.stdout_logfile_maxbytes &&
           a.stdout_logfile_backups == b.stdout_logfile_backups &&
           a.stderr_logfile == b.stderr_logfile &&
           a.stderr_logfile_maxbytes == b.stderr_logfile_maxbytes &&
           a.stderr_logfile_backups == b.stderr_logfile_backups &&
           a.environment == b.environment;
}

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

    bool LoadConfiguration(const std::wstring& configFile) {
        auto parse = ParseConfigDetailed(configFile);
        for (const auto& warning : parse.warnings) {
            Logger::Log("supervisord", "CONFIG WARNING: " + warning);
        }
        for (const auto& error : parse.errors) {
            Logger::Log("supervisord", "CONFIG ERROR: " + error);
            WriteServiceEvent("CONFIG ERROR: " + error, EVENTLOG_ERROR_TYPE);
        }
        if (!parse.errors.empty()) {
            return false;
        }

        config_path_ = configFile;

        std::lock_guard<std::mutex> lock(supervisor_mtx_);
        processes_.clear();
        {
            std::lock_guard<std::mutex> lock(process_lock_map_mtx_);
            process_locks_.clear();
        }

        for (const auto& cfg : parse.configs) {
            processes_[cfg.name] = std::make_unique<ManagedProcess>(cfg);
        }
        Logger::Log("supervisord", "Loaded " + std::to_string(processes_.size()) + " configurations.");
        return true;
    }

    bool ReloadConfiguration(std::string& summary) {
        if (config_path_.empty()) {
            summary = "no config path set";
            return false;
        }

        auto parse = ParseConfigDetailed(config_path_);
        for (const auto& warning : parse.warnings) {
            Logger::Log("supervisord", "CONFIG WARNING: " + warning);
        }
        if (!parse.errors.empty()) {
            for (const auto& error : parse.errors) Logger::Log("supervisord", "CONFIG ERROR: " + error);
            summary = "reload aborted due to config errors";
            return false;
        }

        std::map<std::wstring, ProgramConfig> nextByName;
        for (const auto& cfg : parse.configs) nextByName[cfg.name] = cfg;

        int added = 0, updated = 0, removed = 0;
        std::lock_guard<std::mutex> lock(supervisor_mtx_);

        std::vector<std::wstring> toRemove;
        for (auto& [name, proc] : processes_) {
            if (nextByName.count(name) == 0) toRemove.push_back(name);
        }
        for (const auto& name : toRemove) {
            processes_[name]->Stop();
            processes_.erase(name);
            std::lock_guard<std::mutex> lk(process_lock_map_mtx_);
            process_locks_.erase(name);
            removed++;
        }

        for (auto& [name, cfg] : nextByName) {
            auto it = processes_.find(name);
            if (it == processes_.end()) {
                processes_[name] = std::make_unique<ManagedProcess>(cfg);
                added++;
                if (cfg.autostart) processes_[name]->Start();
                continue;
            }

            if (!ProgramConfigEquals(it->second->config, cfg)) {
                it->second->Stop();
                it->second = std::make_unique<ManagedProcess>(cfg);
                updated++;
                if (cfg.autostart) it->second->Start();
            }
        }

        g_ReloadCount++;
        summary = "reload applied: added=" + std::to_string(added) + " updated=" + std::to_string(updated) + " removed=" + std::to_string(removed);
        Logger::Log("supervisord", summary);
        return true;
    }

    std::vector<std::wstring> BuildDependencyOrderLocked() {
        std::vector<std::wstring> names;
        for (auto& [name, _] : processes_) names.push_back(name);
        std::sort(names.begin(), names.end());

        std::map<std::wstring, int> indegree;
        std::map<std::wstring, std::vector<std::wstring>> adj;
        for (const auto& n : names) indegree[n] = 0;

        for (const auto& [name, proc] : processes_) {
            for (const auto& dep : proc->config.depends_on) {
                if (indegree.count(dep)) {
                    adj[dep].push_back(name);
                    indegree[name]++;
                }
            }
        }

        std::vector<std::wstring> ready;
        for (const auto& [n, d] : indegree) if (d == 0) ready.push_back(n);
        std::sort(ready.begin(), ready.end());

        std::vector<std::wstring> out;
        while (!ready.empty()) {
            std::wstring cur = ready.front();
            ready.erase(ready.begin());
            out.push_back(cur);
            auto itAdj = adj.find(cur);
            if (itAdj == adj.end()) continue;
            for (const auto& nxt : itAdj->second) {
                indegree[nxt]--;
                if (indegree[nxt] == 0) {
                    ready.push_back(nxt);
                    std::sort(ready.begin(), ready.end());
                }
            }
        }

        if (out.size() != names.size()) return names; // cycle fallback
        return out;
    }

    void StartAll() {
        std::lock_guard<std::mutex> lock(supervisor_mtx_);
        auto order = BuildDependencyOrderLocked();
        for (const auto& name : order) {
            auto it = processes_.find(name);
            if (it != processes_.end() && it->second->config.autostart) it->second->Start();
        }
    }

    void Shutdown() {
        Logger::Log("supervisord", "Stopping all managed processes...");
        std::lock_guard<std::mutex> lock(supervisor_mtx_);
        auto order = BuildDependencyOrderLocked();
        std::reverse(order.begin(), order.end());
        std::stable_sort(order.begin(), order.end(), [&](const std::wstring& a, const std::wstring& b) {
            return processes_[a]->config.shutdown_phase > processes_[b]->config.shutdown_phase;
        });
        for (const auto& name : order) {
            auto it = processes_.find(name);
            if (it != processes_.end()) it->second->Stop();
        }
        Logger::Log("supervisord", "Shutdown complete.");
    }

    MetricsSnapshot BuildMetricsSnapshot() {
        MetricsSnapshot snapshot;
        auto now = std::chrono::steady_clock::now();
        snapshot.uptime_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - start_time_).count();
        snapshot.ipc_requests = g_IpcRequests.load();
        snapshot.ipc_errors = g_IpcErrors.load();
        snapshot.reload_count = g_ReloadCount.load();
        snapshot.process_starts = ManagedProcess::g_totalStarts.load();
        snapshot.launch_failures = ManagedProcess::g_totalLaunchFailures.load();
        snapshot.unexpected_exits = ManagedProcess::g_totalUnexpectedExits.load();
        snapshot.restarts = ManagedProcess::g_totalRestarts.load();
        snapshot.healthcheck_failures = ManagedProcess::g_totalHealthCheckFailures.load();
        return snapshot;
    }

    std::string RenderMetricsJson(const MetricsSnapshot& snapshot) const {
        std::stringstream ss;
        ss << "{\n";
        ss << "  \"uptime_seconds\": " << snapshot.uptime_seconds << ",\n";
        ss << "  \"ipc_requests\": " << snapshot.ipc_requests << ",\n";
        ss << "  \"ipc_errors\": " << snapshot.ipc_errors << ",\n";
        ss << "  \"reload_count\": " << snapshot.reload_count << ",\n";
        ss << "  \"process_starts\": " << snapshot.process_starts << ",\n";
        ss << "  \"launch_failures\": " << snapshot.launch_failures << ",\n";
        ss << "  \"unexpected_exits\": " << snapshot.unexpected_exits << ",\n";
        ss << "  \"restarts\": " << snapshot.restarts << ",\n";
        ss << "  \"healthcheck_failures\": " << snapshot.healthcheck_failures << "\n";
        ss << "}\n";
        return ss.str();
    }

    std::vector<StatusSnapshotEntry> BuildStatusSnapshot() {
        std::vector<StatusSnapshotEntry> snapshot;
        std::lock_guard<std::mutex> lock(supervisor_mtx_);
        snapshot.reserve(processes_.size());
        for (auto& [name, proc] : processes_) {
            snapshot.push_back({WideToUtf8(name), StateToString(proc->GetState())});
        }
        return snapshot;
    }

    std::string RenderStatusSnapshot(const std::vector<StatusSnapshotEntry>& snapshot) const {
        std::stringstream res;
        for (const auto& entry : snapshot) {
            res << std::left << std::setw(25) << entry.name << std::setw(15) << entry.state << "\n";
        }
        return res.str();
    }

    std::string BuildMetricsJson() {
        return RenderMetricsJson(BuildMetricsSnapshot());
    }

    void WriteMetricsFile() {
        std::wstring path = GetExecutableDir() + L"\\supervisord.metrics.json";
        std::ofstream out(path, std::ios::trunc | std::ios::binary);
        if (!out.is_open()) return;
        std::string content = BuildMetricsJson();
        out.write(content.data(), content.size());
    }

    std::mutex& GetProcessCommandLock(const std::wstring& processName) {
        std::lock_guard<std::mutex> lock(process_lock_map_mtx_);
        auto it = process_locks_.find(processName);
        if (it == process_locks_.end()) {
            auto inserted = process_locks_.emplace(processName, std::make_unique<std::mutex>());
            return *(inserted.first->second);
        }
        return *(it->second);
    }

    // Lock ordering policy:
    // 1) process-specific command mutex from GetProcessCommandLock
    // 2) supervisor_mtx_
    bool ExecuteWithProcessLocked(const std::wstring& processName, const std::function<void(ManagedProcess&)>& fn) {
        std::lock_guard<std::mutex> processGuard(GetProcessCommandLock(processName));
        std::lock_guard<std::mutex> lock(supervisor_mtx_);
        auto it = processes_.find(processName);
        if (it == processes_.end()) return false;
        fn(*it->second);
        return true;
    }

    std::string HandleIpcCommand(const std::string& cmd, bool clientPrivileged) {
        g_IpcRequests++;
        IpcRequest req;
        req.raw = cmd;
        req.clientPrivileged = clientPrivileged;
        req.tokens = ParseIpcCommandTokens(cmd);
        if (req.tokens.empty()) {
            g_IpcErrors++;
            return IpcResponse::Err(IpcCodes::kErrInvalidSyntax, "empty command").ToFrame();
        }

        req.actionText = req.tokens[0];
        req.action = ParseIpcAction(req.actionText);
        if (req.tokens.size() > 1) {
            req.target = req.tokens[1];
            req.wTarget = Utf8ToWide(req.target);
        }

        auto requirePrivileged = [&req]() -> bool {
            return req.clientPrivileged || IsCurrentProcessElevatedOrAdmin();
        };

        std::map<IpcAction, std::function<IpcResponse()>> handlers;
        handlers[IpcAction::Status] = [&]() {
            return IpcResponse::OkWithPayload(IpcCodes::kOkStatus, RenderStatusSnapshot(BuildStatusSnapshot()));
        };
        handlers[IpcAction::Diag] = [&]() {
            size_t limit = SupervisorDefaults::kDefaultDiagLimit;
            if (req.tokens.size() > 1) {
                try { limit = static_cast<size_t>(std::stoul(req.tokens[1])); } catch (...) { limit = SupervisorDefaults::kDefaultDiagLimit; }
            }
            return IpcResponse::OkWithPayload(IpcCodes::kOkDiag, EventRingBuffer::Dump(limit));
        };
        handlers[IpcAction::Metrics] = [&]() {
            return IpcResponse::OkWithPayload(IpcCodes::kOkMetrics, BuildMetricsJson());
        };
        handlers[IpcAction::Reload] = [&]() {
            if (!requirePrivileged()) return IpcResponse::Err(IpcCodes::kErrAccessDenied, "reload requires admin privileges");
            std::string summary;
            if (ReloadConfiguration(summary)) return IpcResponse::Ok(IpcCodes::kOkReloaded, summary);
            return IpcResponse::Err(IpcCodes::kErrReloadFailed, summary);
        };
        handlers[IpcAction::Start] = [&]() {
            if (!requirePrivileged()) return IpcResponse::Err(IpcCodes::kErrAccessDenied, "start requires admin privileges");
            if (req.target.empty()) return IpcResponse::Err(IpcCodes::kErrMissingTarget, "start requires process_name");
            if (!ExecuteWithProcessLocked(req.wTarget, [](ManagedProcess& p) { p.Start(); })) {
                return IpcResponse::Err(IpcCodes::kErrNoSuchProcess, req.target);
            }
            return IpcResponse::Ok(IpcCodes::kOkStarted, req.target);
        };
        handlers[IpcAction::Stop] = [&]() {
            if (!requirePrivileged()) return IpcResponse::Err(IpcCodes::kErrAccessDenied, "stop requires admin privileges");
            if (req.target.empty()) return IpcResponse::Err(IpcCodes::kErrMissingTarget, "stop requires process_name");
            if (!ExecuteWithProcessLocked(req.wTarget, [](ManagedProcess& p) { p.Stop(); })) {
                return IpcResponse::Err(IpcCodes::kErrNoSuchProcess, req.target);
            }
            return IpcResponse::Ok(IpcCodes::kOkStopped, req.target);
        };
        handlers[IpcAction::Restart] = [&]() {
            if (!requirePrivileged()) return IpcResponse::Err(IpcCodes::kErrAccessDenied, "restart requires admin privileges");
            if (req.target.empty()) return IpcResponse::Err(IpcCodes::kErrMissingTarget, "restart requires process_name");
            if (!ExecuteWithProcessLocked(req.wTarget, [](ManagedProcess& p) { p.Stop(); p.Start(); })) {
                return IpcResponse::Err(IpcCodes::kErrNoSuchProcess, req.target);
            }
            return IpcResponse::Ok(IpcCodes::kOkRestarted, req.target);
        };

        auto it = handlers.find(req.action);
        IpcResponse response;
        if (it == handlers.end()) {
            response = IpcResponse::Err(IpcCodes::kErrUnknownCommand, req.actionText);
        } else {
            response = it->second();
        }
        if (!response.ok) g_IpcErrors++;
        return response.ToFrame();
    }

    void RunIpcServer() {
        ipc_thread_ = std::thread([this]() {
            PSECURITY_DESCRIPTOR pSD = NULL;
            // SDDL: Allow access to Local System (SY), Built-in Administrators (BA), and Authenticated Users (AU)
            ConvertStringSecurityDescriptorToSecurityDescriptorW(
                L"D:(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGW;;;AU)", SDDL_REVISION_1, &pSD, NULL);

            SECURITY_ATTRIBUTES sa = { sizeof(SECURITY_ATTRIBUTES), pSD, FALSE };

            while (!g_SignalReceived) {
                ScopedHandle hPipe(CreateNamedPipeW(
                    PIPE_NAME, PIPE_ACCESS_DUPLEX,
                    PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                    1, SupervisorDefaults::kPipeOutBuffer, SupervisorDefaults::kPipeInBuffer, 0, &sa
                ));

                if (!hPipe.isValid()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }

                if (ConnectNamedPipe(hPipe.get(), NULL) || GetLastError() == ERROR_PIPE_CONNECTED) {
                    if (g_SignalReceived) break;
                    std::string request;
                    if (ReadIpcFrame(hPipe.get(), request)) {
                        bool privileged = IsPipeClientPrivileged(hPipe.get());
                        std::string reply = HandleIpcCommand(request, privileged);
                        WriteIpcFrame(hPipe.get(), reply);
                    } else {
                        WriteIpcFrame(hPipe.get(), IpcResponse::Err(IpcCodes::kErrBadFrame, "invalid IPC frame").ToFrame());
                    }
                    FlushFileBuffers(hPipe.get());
                }
                DisconnectNamedPipe(hPipe.get());
            }

            if (pSD) LocalFree(pSD);
        });
    }

    void JoinIpcServer() {
        g_SignalReceived = true;
        // Unblock ConnectNamedPipe by pinging the pipe locally
        HANDLE hPipe = CreateFileW(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) CloseHandle(hPipe);

        if (ipc_thread_.joinable()) ipc_thread_.join();
    }

private:
    std::mutex supervisor_mtx_;
    std::map<std::wstring, std::unique_ptr<ManagedProcess>> processes_;
    std::mutex process_lock_map_mtx_;
    std::map<std::wstring, std::unique_ptr<std::mutex>> process_locks_;
    std::wstring config_path_;
    std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::now();
    std::thread ipc_thread_;
};

int SendIpcClientCommand(const std::string& cmd) {
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    constexpr int kMaxAttempts = SupervisorDefaults::kIpcConnectRetryCount;

    for (int attempt = 0; attempt < kMaxAttempts; ++attempt) {
        hPipe = CreateFileW(PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (hPipe != INVALID_HANDLE_VALUE) break;

        DWORD err = GetLastError();
        if (err != ERROR_PIPE_BUSY && err != ERROR_FILE_NOT_FOUND) {
            std::cout << "Error: Could not connect to supervisord daemon. Win32 Error: " << err << "\n";
            return 10;
        }

        if (!WaitNamedPipeW(PIPE_NAME, SupervisorDefaults::kIpcBusyTimeoutMs)) {
            std::cout << "Error: Supervisord daemon pipe unavailable/busy timeout.\n";
            return 11;
        }
    }

    if (hPipe == INVALID_HANDLE_VALUE) {
        std::cout << "Error: Could not connect to supervisord daemon after retries.\n";
        return 12;
    }

    DWORD written = 0;
    if (!WriteIpcFrame(hPipe, cmd)) {
        std::cout << "Error: Failed to send command to daemon. Win32 Error: " << GetLastError() << "\n";
        CloseHandle(hPipe);
        return 13;
    }

    std::string response;
    if (!ReadIpcFrame(hPipe, response)) {
        std::cout << "Error: Failed to read framed response from daemon.\n";
        CloseHandle(hPipe);
        return 15;
    }
    CloseHandle(hPipe);

    if (response.empty()) {
        std::cout << "Error: Empty response from daemon.\n";
        return 14;
    }

    size_t nl = response.find('\n');
    std::string header = (nl == std::string::npos) ? response : response.substr(0, nl);
    std::string payload = (nl == std::string::npos) ? std::string() : response.substr(nl + 1);

    if (header.rfind("OK|", 0) == 0) {
        if (header.rfind("OK|STATUS", 0) == 0 || header.rfind("OK|DIAG", 0) == 0 || header.rfind("OK|METRICS", 0) == 0) {
            std::cout << payload;
        } else {
            std::vector<std::string> parts;
            std::stringstream hs(header);
            std::string piece;
            while (std::getline(hs, piece, '|')) parts.push_back(piece);
            if (parts.size() >= 3) std::cout << parts[2] << "\n";
            else std::cout << "OK\n";
        }
        return 0;
    }

    if (header.rfind("ERR|", 0) == 0) {
        std::vector<std::string> parts;
        std::stringstream hs(header);
        std::string piece;
        while (std::getline(hs, piece, '|')) parts.push_back(piece);
        if (parts.size() >= 3) {
            std::cout << "Error [" << parts[1] << "]: " << parts[2] << "\n";
        } else {
            std::cout << "Error: " << header << "\n";
        }
        return 20;
    }

    std::cout << response;
    return 0;
}

// ============================================================================
// SECTION: SERVICE_AND_ENTRYPOINT
// Contract:
// - Own SCM glue and process startup mode selection only.
// - No business logic duplication; delegate to Supervisord methods.
// ============================================================================
SERVICE_STATUS        g_SvcStatus = {0};
SERVICE_STATUS_HANDLE g_SvcStatusHandle = NULL;
Supervisord           g_Supervisor;

VOID WINAPI SvcCtrlHandler(DWORD dwCtrl) {
    if (dwCtrl == SERVICE_CONTROL_STOP || dwCtrl == SERVICE_CONTROL_SHUTDOWN) {
        g_SvcStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_SvcStatusHandle, &g_SvcStatus);
        Supervisord::g_SignalReceived = true;
    }
}

VOID WINAPI SvcMain(DWORD argc, LPTSTR* argv) {
    g_IsServiceMode = true;
    g_SvcStatusHandle = RegisterServiceCtrlHandlerW(L"Supervisord", SvcCtrlHandler);
    if (!g_SvcStatusHandle) return;

    g_SvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_SvcStatus.dwCurrentState = SERVICE_RUNNING;
    g_SvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    SetServiceStatus(g_SvcStatusHandle, &g_SvcStatus);

    std::wstring configPath = GetUserHomeDir() + L"\\supervisord.conf";
    if (!g_Supervisor.LoadConfiguration(configPath)) {
        g_SvcStatus.dwCurrentState = SERVICE_STOPPED;
        g_SvcStatus.dwWin32ExitCode = ERROR_INVALID_DATA;
        SetServiceStatus(g_SvcStatusHandle, &g_SvcStatus);
        return;
    }
    g_Supervisor.StartAll();
    g_Supervisor.RunIpcServer();

    while (!Supervisord::g_SignalReceived) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    g_Supervisor.Shutdown();
    g_Supervisor.JoinIpcServer();
    g_Supervisor.WriteMetricsFile();
    RotatingFileSink::ReportStats();

    g_SvcStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_SvcStatusHandle, &g_SvcStatus);
}

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_CLOSE_EVENT || ctrlType == CTRL_SHUTDOWN_EVENT) {
        Supervisord::g_SignalReceived = true;
        return TRUE;
    }
    // Ignore CTRL_BREAK_EVENT so targeted child process breaks don't kill supervisor
    return TRUE;
}

void ManageServiceRegistration(bool install) {
    SC_HANDLE schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (!schSCManager) { std::cout << "Failed to open SCManager.\n"; return; }

    if (install) {
        std::wstring exePath = GetExecutableDir() + L"\\supervisord.exe";
        std::wstring cmd = L"\"" + exePath + L"\" --service";
        SC_HANDLE schService = CreateServiceW(
            schSCManager, L"Supervisord", L"Windows Native Supervisor Service",
            SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
            SERVICE_ERROR_NORMAL, cmd.c_str(), NULL, NULL, NULL, NULL, NULL
        );
        if (schService) {
            std::cout << "Service installed successfully.\n";
            CloseServiceHandle(schService);
        } else {
            std::cout << "Failed to install service. Error: " << GetLastError() << "\n";
        }
    } else {
        SC_HANDLE schService = OpenServiceW(schSCManager, L"Supervisord", DELETE);
        if (schService) {
            if (DeleteService(schService)) std::cout << "Service uninstalled successfully.\n";
            else std::cout << "Failed to delete service.\n";
            CloseServiceHandle(schService);
        }
    }
    CloseServiceHandle(schSCManager);
}


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

int wmain(int argc, wchar_t* argv[]) {
    if (argc > 1) {
        std::wstring arg1 = argv[1];
        if (arg1 == L"--help" || arg1 == L"-h" || arg1 == L"help" || arg1 == L"/?" || arg1 == L"-?") {
            PrintHelp();
            return 0;
        } else if (arg1 == L"--version" || arg1 == L"-V" || arg1 == L"-v") {
            PrintVersion();
            return 0;
        } else if (arg1 == L"--check-config") {
            std::wstring configPath = (argc > 2) ? argv[2] : (GetUserHomeDir() + L"\\supervisord.conf");
            auto parsed = ParseConfigDetailed(configPath);
            std::cout << "Config path: " << WideToUtf8(configPath) << "\n";
            for (const auto& w : parsed.warnings) std::cout << "WARN: " << w << "\n";
            for (const auto& e : parsed.errors) std::cout << "ERROR: " << e << "\n";
            std::cout << "Programs loaded: " << parsed.configs.size() << "\n";
            return parsed.errors.empty() ? 0 : 2;
        } else if (arg1 == L"--service") {
            SERVICE_TABLE_ENTRYW ServiceTable[] = {
                { (LPWSTR)L"Supervisord", (LPSERVICE_MAIN_FUNCTIONW)SvcMain },
                { NULL, NULL }
            };
            StartServiceCtrlDispatcherW(ServiceTable);
            return 0;
        } else if (arg1 == L"--install-service") {
            ManageServiceRegistration(true);
            return 0;
        } else if (arg1 == L"--uninstall-service") {
            ManageServiceRegistration(false);
            return 0;
        } else if (arg1 == L"status" || arg1 == L"diag" || arg1 == L"metrics" || arg1 == L"reload" || arg1 == L"start" || arg1 == L"stop" || arg1 == L"restart") {
            std::string cmd = WideToUtf8(arg1);
            for (int i = 2; i < argc; ++i) {
                std::wstring argN = argv[i];
                cmd += " " + QuoteForIpc(WideToUtf8(argN));
            }
            return SendIpcClientCommand(cmd);
        } else {
            std::wcout << L"Unknown command: " << arg1 << L"\n\n";
            PrintHelp();
            return 1;
        }
    }

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
    Logger::Log("supervisord", "Starting standalone console daemon...");

    std::wstring configPath = GetUserHomeDir() + L"\\supervisord.conf";
    if (!g_Supervisor.LoadConfiguration(configPath)) {
        std::cout << "Configuration errors prevented startup. Run --check-config for details.\n";
        return 2;
    }
    g_Supervisor.StartAll();
    g_Supervisor.RunIpcServer();

    while (!Supervisord::g_SignalReceived) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    g_Supervisor.Shutdown();
    g_Supervisor.JoinIpcServer();
    g_Supervisor.WriteMetricsFile();
    RotatingFileSink::ReportStats();
    return 0;
}