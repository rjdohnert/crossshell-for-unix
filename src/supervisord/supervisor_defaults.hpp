#pragma once

#include "supervisord.hpp"

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
