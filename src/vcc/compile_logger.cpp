#include "compile_logger.hpp"

std::string CompileLogger::GetLogPath() {
        char buf[MAX_PATH] = {};
        if (GetEnvironmentVariableA("USERPROFILE", buf, MAX_PATH) > 0 && buf[0] != '\0') {
            return std::string(buf) + "\\.vcc_log";
        }
        char drive[16] = {};
        char path[MAX_PATH] = {};
        if (GetEnvironmentVariableA("HOMEDRIVE", drive, sizeof(drive)) > 0 &&
            GetEnvironmentVariableA("HOMEPATH", path, sizeof(path)) > 0) {
            return std::string(drive) + std::string(path) + "\\.vcc_log";
        }
        if (GetEnvironmentVariableA("HOME", buf, MAX_PATH) > 0 && buf[0] != '\0') {
            return std::string(buf) + "\\.vcc_log";
        }
        return std::string();
    }

std::string CompileLogger::GetTimestamp() {
        SYSTEMTIME st;
        GetLocalTime(&st);
        char ts[64] = {};
        snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
                 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        return std::string(ts);
    }

void CompileLogger::LogInvocation(const std::string& commandLine) {
        std::string logPath = GetLogPath();
        if (logPath.empty()) return;

        std::ofstream log(logPath, std::ios::app);
        if (!log.is_open()) return;

        char cwd[MAX_PATH] = {};
        GetCurrentDirectoryA(MAX_PATH, cwd);

        log << "[" << GetTimestamp() << "]"
            << " cwd=\"" << cwd << "\""
            << " cmd=" << commandLine << "\n";
    }

void CompileLogger::LogResult(int exitCode) {
        std::string logPath = GetLogPath();
        if (logPath.empty()) return;

        std::ofstream log(logPath, std::ios::app);
        if (!log.is_open()) return;

        log << "[" << GetTimestamp() << "]"
            << " exit=" << exitCode << "\n";
    }
