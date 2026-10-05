/*
 * Copyright (c) 2026 PC/OpenSystems LLC contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
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


#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <tlhelp32.h>
#include <sddl.h>

#pragma comment(lib, "advapi32.lib")

#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <optional>
#include <iomanip>
#include <algorithm>
#include <sstream>

// ============================================================================
// RAII System Wrappers
// ============================================================================

class ScopedHandle {
public:
    explicit ScopedHandle(HANDLE h = INVALID_HANDLE_VALUE) noexcept : m_handle(h) {}
    ~ScopedHandle() noexcept { reset(); }

    ScopedHandle(const ScopedHandle&) = delete;
    ScopedHandle& operator=(const ScopedHandle&) = delete;

    ScopedHandle(ScopedHandle&& other) noexcept : m_handle(other.release()) {}
    ScopedHandle& operator=(ScopedHandle&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    [[nodiscard]] HANDLE get() const noexcept { return m_handle; }
    [[nodiscard]] bool isValid() const noexcept { 
        return m_handle != INVALID_HANDLE_VALUE && m_handle != nullptr; 
    }

    HANDLE release() noexcept {
        HANDLE tmp = m_handle;
        m_handle = INVALID_HANDLE_VALUE;
        return tmp;
    }

    void reset(HANDLE h = INVALID_HANDLE_VALUE) noexcept {
        if (isValid()) {
            ::CloseHandle(m_handle);
        }
        m_handle = h;
    }

private:
    HANDLE m_handle;
};

// ============================================================================
// Priority Mapper
// ============================================================================

class PriorityMapper {
public:
    static std::optional<DWORD> fromNiceValue(int nice) noexcept {
        if (nice < -20 || nice > 20) return std::nullopt;
        if (nice <= -15) return REALTIME_PRIORITY_CLASS;
        if (nice <= -6)  return HIGH_PRIORITY_CLASS;
        if (nice <= -1)  return ABOVE_NORMAL_PRIORITY_CLASS;
        if (nice == 0)   return NORMAL_PRIORITY_CLASS;
        if (nice <= 9)   return BELOW_NORMAL_PRIORITY_CLASS;
        return IDLE_PRIORITY_CLASS;
    }

    static std::optional<DWORD> fromString(std::string_view name) noexcept {
        std::string lower(name);
        std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        if (lower == "realtime" || lower == "rt")       return REALTIME_PRIORITY_CLASS;
        if (lower == "high" || lower == "h")           return HIGH_PRIORITY_CLASS;
        if (lower == "abovenormal" || lower == "an")   return ABOVE_NORMAL_PRIORITY_CLASS;
        if (lower == "normal" || lower == "n")         return NORMAL_PRIORITY_CLASS;
        if (lower == "belownormal" || lower == "bn")   return BELOW_NORMAL_PRIORITY_CLASS;
        if (lower == "idle" || lower == "low" || lower == "i") return IDLE_PRIORITY_CLASS;

        return std::nullopt;
    }

    static std::string toString(DWORD priorityClass) {
        switch (priorityClass) {
            case REALTIME_PRIORITY_CLASS:     return "Realtime (-20)";
            case HIGH_PRIORITY_CLASS:         return "High (-10)";
            case ABOVE_NORMAL_PRIORITY_CLASS: return "AboveNormal (-5)";
            case NORMAL_PRIORITY_CLASS:       return "Normal (0)";
            case BELOW_NORMAL_PRIORITY_CLASS: return "BelowNormal (5)";
            case IDLE_PRIORITY_CLASS:         return "Idle (15)";
            default:                          return "Unknown";
        }
    }
};

// ============================================================================
// Process Information Model
// ============================================================================

struct ProcessInfo {
    DWORD pid{0};
    std::wstring name;
    std::wstring owner;
    DWORD currentPriorityClass{0};
};

// ============================================================================
// Privilege Manager
// ============================================================================

class PrivilegeManager {
public:
    static bool enableDebugPrivilege() noexcept {
        ScopedHandle hToken;
        HANDLE rawToken = nullptr;
        if (!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &rawToken)) {
            return false;
        }
        hToken.reset(rawToken);

        LUID luid;
        if (!::LookupPrivilegeValueW(nullptr, L"SeDebugPrivilege", &luid)) {
            return false;
        }

        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        return ::AdjustTokenPrivileges(hToken.get(), FALSE, &tp, sizeof(tp), nullptr, nullptr) &&
               (::GetLastError() == ERROR_SUCCESS);
    }
};

// ============================================================================
// Process Inspector & Filter Strategy
// ============================================================================

class IProcessFilter {
public:
    virtual ~IProcessFilter() = default;
    [[nodiscard]] virtual bool matches(const ProcessInfo& proc) const = 0;
};

class PidFilter : public IProcessFilter {
public:
    explicit PidFilter(DWORD pid) : m_pid(pid) {}
    [[nodiscard]] bool matches(const ProcessInfo& proc) const override {
        return proc.pid == m_pid;
    }
private:
    DWORD m_pid;
};

class NameFilter : public IProcessFilter {
public:
    explicit NameFilter(std::wstring name) : m_name(std::move(name)) {
        std::transform(m_name.begin(), m_name.end(), m_name.begin(), ::towlower);
    }
    [[nodiscard]] bool matches(const ProcessInfo& proc) const override {
        std::wstring current = proc.name;
        std::transform(current.begin(), current.end(), current.begin(), ::towlower);
        return current == m_name;
    }
private:
    std::wstring m_name;
};

class UserFilter : public IProcessFilter {
public:
    explicit UserFilter(std::wstring user) : m_user(std::move(user)) {
        std::transform(m_user.begin(), m_user.end(), m_user.begin(), ::towlower);
    }
    [[nodiscard]] bool matches(const ProcessInfo& proc) const override {
        std::wstring current = proc.owner;
        std::transform(current.begin(), current.end(), current.begin(), ::towlower);
        return current == m_user;
    }
private:
    std::wstring m_user;
};

// ============================================================================
// Process Controller
// ============================================================================

class ProcessController {
public:
    [[nodiscard]] static std::wstring getProcessOwner(HANDLE hProcess) {
        ScopedHandle hToken;
        HANDLE rawToken = nullptr;
        if (!::OpenProcessToken(hProcess, TOKEN_QUERY, &rawToken)) {
            return L"<Unknown>";
        }
        hToken.reset(rawToken);

        DWORD len = 0;
        ::GetTokenInformation(hToken.get(), TokenUser, nullptr, 0, &len);
        if (::GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
            return L"<Unknown>";
        }

        std::vector<BYTE> buffer(len);
        if (!::GetTokenInformation(hToken.get(), TokenUser, buffer.data(), len, &len)) {
            return L"<Unknown>";
        }

        auto* tokenUser = reinterpret_cast<TOKEN_USER*>(buffer.data());
        WCHAR name[256];
        DWORD nameLen = 256;
        WCHAR domain[256];
        DWORD domainLen = 256;
        SID_NAME_USE use;

        if (::LookupAccountSidW(nullptr, tokenUser->User.Sid, name, &nameLen, domain, &domainLen, &use)) {
            return std::wstring(domain) + L"\\" + std::wstring(name);
        }
        return L"<Unknown>";
    }

    [[nodiscard]] static std::vector<ProcessInfo> snapshotProcesses() {
        std::vector<ProcessInfo> list;
        ScopedHandle snapshot(::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
        if (!snapshot.isValid()) {
            return list;
        }

        PROCESSENTRY32W entry{};
        entry.dwSize = sizeof(entry);

        if (::Process32FirstW(snapshot.get(), &entry)) {
            do {
                if (entry.th32ProcessID == 0) continue;

                ProcessInfo info;
                info.pid = entry.th32ProcessID;
                info.name = entry.szExeFile;

                ScopedHandle hProc(::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID));
                if (hProc.isValid()) {
                    info.currentPriorityClass = ::GetPriorityClass(hProc.get());
                    info.owner = getProcessOwner(hProc.get());
                } else {
                    info.currentPriorityClass = 0;
                    info.owner = L"<Access Denied>";
                }
                list.push_back(std::move(info));
            } while (::Process32NextW(snapshot.get(), &entry));
        }
        return list;
    }

    static bool applyPriority(DWORD pid, DWORD targetPriority, std::string& errorMsg) {
        ScopedHandle hProc(::OpenProcess(PROCESS_SET_INFORMATION | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid));
        if (!hProc.isValid()) {
            DWORD err = ::GetLastError();
            errorMsg = "OpenProcess failed (Error code: " + std::to_string(err) + ")";
            return false;
        }

        if (!::SetPriorityClass(hProc.get(), targetPriority)) {
            DWORD err = ::GetLastError();
            errorMsg = "SetPriorityClass failed (Error code: " + std::to_string(err) + ")";
            return false;
        }

        return true;
    }
};

// ============================================================================
// Application Core
// ============================================================================

class ReniceApplication {
public:
    ReniceApplication() {
        PrivilegeManager::enableDebugPrivilege();
    }

    void printHelp(std::string_view /*execName*/ = "renice") const {
        std::cout << R"(renice(1)                  CrossShell for UNIX Reference Manual                 renice(1)

    NAME
        renice - alter priority of running processes

    SYNOPSIS
        renice <priority> [OPTIONS] <target...>

    DESCRIPTION
        renice alters the scheduling priority class of one or more running
        processes on Windows. <priority> can be provided as an integer nice
        value (-20 to 20) or as a Windows priority class name (realtime, high,
        abovenormal, normal, belownormal, idle).

    OPTIONS
        -p, --pid <pid...>
            Interpret following arguments as Process IDs (default mode).

        -u, --user <user...>
            Alter all processes owned by the given user account name.

        -n, --name <name...>
            Alter all processes matching the executable image name (e.g., notepad.exe).

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        renice -5 4120 8912
            Set processes with PID 4120 and 8912 to AboveNormal priority (-5).

        renice 10 -n chrome.exe
            Lower all running instances of chrome.exe to Idle/Low priority.

        renice high -u Alice
            Set all processes owned by user Alice to High priority.

        renice normal -p 1044 -n svchost.exe
            Set PID 1044 and all svchost.exe instances back to Normal priority.

    CrossShell for UNIX                                                          renice(1)
)";
    }

    int run(int argc, char* argv[]) {
        if (argc < 2) {
            printHelp(argv[0]);
            return 1;
        }

        std::string firstArg = argv[1];
        if (firstArg == "-h" || firstArg == "--help" || firstArg == "/?") {
            printHelp(argv[0]);
            return 0;
        }

        if (argc < 3) {
            std::cerr << "renice: error: missing target processes or priority.\n"
                      << "Try '" << argv[0] << " --help' for more information.\n";
            return 1;
        }

        // Parse Target Priority
        DWORD targetPriority = 0;
        if (auto p = PriorityMapper::fromString(firstArg)) {
            targetPriority = *p;
        } else {
            try {
                int niceVal = std::stoi(firstArg);
                if (auto pNice = PriorityMapper::fromNiceValue(niceVal)) {
                    targetPriority = *pNice;
                } else {
                    std::cerr << "renice: error: nice value " << niceVal << " is out of range [-20, 20].\n";
                    return 1;
                }
            } catch (...) {
                std::cerr << "renice: error: invalid priority specification: '" << firstArg << "'.\n";
                return 1;
            }
        }

        // Build target filters based on CLI flags
        enum class ParseMode { PID, USER, NAME };
        ParseMode mode = ParseMode::PID;

        std::vector<std::unique_ptr<IProcessFilter>> filters;

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-p" || arg == "--pid") {
                mode = ParseMode::PID;
            } else if (arg == "-u" || arg == "--user") {
                mode = ParseMode::USER;
            } else if (arg == "-n" || arg == "--name") {
                mode = ParseMode::NAME;
            } else {
                if (mode == ParseMode::PID) {
                    try {
                        DWORD pid = static_cast<DWORD>(std::stoul(arg));
                        filters.push_back(std::make_unique<PidFilter>(pid));
                    } catch (...) {
                        std::cerr << "renice: error: invalid pid '" << arg << "'.\n";
                    }
                } else if (mode == ParseMode::USER) {
                    std::wstring wuser(arg.begin(), arg.end());
                    filters.push_back(std::make_unique<UserFilter>(wuser));
                } else if (mode == ParseMode::NAME) {
                    std::wstring wname(arg.begin(), arg.end());
                    filters.push_back(std::make_unique<NameFilter>(wname));
                }
            }
        }

        if (filters.empty()) {
            std::cerr << "renice: error: no valid target filters provided.\n";
            return 1;
        }

        auto processList = ProcessController::snapshotProcesses();
        int matchedCount = 0;
        int successCount = 0;

        for (const auto& proc : processList) {
            bool matched = false;
            for (const auto& filter : filters) {
                if (filter->matches(proc)) {
                    matched = true;
                    break;
                }
            }

            if (matched) {
                ++matchedCount;
                std::string err;
                std::string oldPrio = PriorityMapper::toString(proc.currentPriorityClass);
                std::string newPrio = PriorityMapper::toString(targetPriority);

                auto to_narrow = [](const std::wstring& wstr) -> std::string {
                    if (wstr.empty()) return {};
                    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
                    std::string result(size, '\0');
                    WideCharToMultiByte(CP_UTF8, 0, wstr.data(), static_cast<int>(wstr.size()), result.data(), size, nullptr, nullptr);
                    return result;
                };

                if (ProcessController::applyPriority(proc.pid, targetPriority, err)) {
                    ++successCount;
                    std::cout << "[SUCCESS] PID " << std::setw(6) << proc.pid 
                              << " (" << to_narrow(proc.name) << "): "
                              << oldPrio << " -> " << newPrio << "\n";
                } else {
                    std::cerr << "[FAILED]  PID " << std::setw(6) << proc.pid 
                              << " (" << to_narrow(proc.name) << "): "
                              << err << "\n";
                }
            }
        }

        if (matchedCount == 0) {
            std::cerr << "renice: no matching processes found.\n";
            return 1;
        }

        return (matchedCount == successCount) ? 0 : 1;
    }
};

// ============================================================================
// Entry Point
// ============================================================================

int main(int argc, char* argv[]) {
    ReniceApplication app;
    return app.run(argc, argv);
}