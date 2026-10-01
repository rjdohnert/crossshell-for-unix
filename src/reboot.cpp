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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <string>
#include <algorithm>
#include <windows.h>
#include <shellapi.h>
#include <memory>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")

// ============================================================================
// 1. RAII HANDLES & PRIVILEGE GUARDS
// ============================================================================

class ScopedTokenHandle {
public:
    explicit ScopedTokenHandle(HANDLE handle = NULL) : m_handle(handle) {}

    ~ScopedTokenHandle() {
        Close();
    }

    ScopedTokenHandle(const ScopedTokenHandle&) = delete;
    ScopedTokenHandle& operator=(const ScopedTokenHandle&) = delete;

    ScopedTokenHandle(ScopedTokenHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = NULL;
    }

    ScopedTokenHandle& operator=(ScopedTokenHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = NULL;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    HANDLE* Receive() { Close(); return &m_handle; }
    bool IsValid() const { return m_handle != NULL && m_handle != INVALID_HANDLE_VALUE; }

    void Close() {
        if (m_handle != NULL && m_handle != INVALID_HANDLE_VALUE) {
            CloseHandle(m_handle);
            m_handle = NULL;
        }
    }

private:
    HANDLE m_handle;
};

class ScopedSidHandle {
public:
    explicit ScopedSidHandle(PSID sid = NULL) : m_sid(sid) {}

    ~ScopedSidHandle() {
        Free();
    }

    ScopedSidHandle(const ScopedSidHandle&) = delete;
    ScopedSidHandle& operator=(const ScopedSidHandle&) = delete;

    ScopedSidHandle(ScopedSidHandle&& other) noexcept : m_sid(other.m_sid) {
        other.m_sid = NULL;
    }

    ScopedSidHandle& operator=(ScopedSidHandle&& other) noexcept {
        if (this != &other) {
            Free();
            m_sid = other.m_sid;
            other.m_sid = NULL;
        }
        return *this;
    }

    PSID Get() const { return m_sid; }
    PSID* Receive() { Free(); return &m_sid; }
    bool IsValid() const { return m_sid != NULL; }

    void Free() {
        if (m_sid != NULL) {
            FreeSid(m_sid);
            m_sid = NULL;
        }
    }

private:
    PSID m_sid;
};

class PrivilegeManager {
public:
    static bool EnableShutdownPrivilege() {
        ScopedTokenHandle hToken;
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, hToken.Receive())) {
            return false;
        }

        TOKEN_PRIVILEGES tkp = {};
        if (!LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, &tkp.Privileges[0].Luid)) {
            return false;
        }

        tkp.PrivilegeCount = 1;
        tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

        if (!AdjustTokenPrivileges(hToken.Get(), FALSE, &tkp, 0, nullptr, nullptr)) {
            return false;
        }

        return (GetLastError() == ERROR_SUCCESS);
    }

    static bool IsRunningAsAdmin() {
        BOOL isAdmin = FALSE;
        SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
        ScopedSidHandle adminGroup;

        if (AllocateAndInitializeSid(
                &ntAuthority,
                2,
                SECURITY_BUILTIN_DOMAIN_RID,
                DOMAIN_ALIAS_RID_ADMINS,
                0,
                0,
                0,
                0,
                0,
                0,
                adminGroup.Receive())) {
            CheckTokenMembership(NULL, adminGroup.Get(), &isAdmin);
        }

        return isAdmin == TRUE;
    }
};

class ElevationManager {
public:
    static bool RelaunchElevated() {
        char exePath[MAX_PATH] = {0};
        if (GetModuleFileNameA(NULL, exePath, MAX_PATH) == 0) {
            return false;
        }

        SHELLEXECUTEINFOA sei = {0};
        sei.cbSize = sizeof(sei);
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.hwnd = NULL;
        sei.lpVerb = "runas";
        sei.lpFile = exePath;
        sei.lpParameters = "--elevated";
        sei.nShow = SW_SHOWNORMAL;

        return ShellExecuteExA(&sei) != FALSE;
    }
};

// ============================================================================
// 2. OPTIONS PARSER
// ============================================================================

class RebootOptions {
public:
    bool elevatedRun = false;
    bool suppressWall = false;
    bool force = false;
    bool wtmpOnly = false;
    bool showHelp = false;
    bool showVersion = false;

    int Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                showHelp = true;
                return 0;
            }
            if (arg == "--version") {
                showVersion = true;
                return 0;
            }
            if (arg == "--elevated") {
                elevatedRun = true;
                continue;
            }
            if (arg == "-n" || arg == "--no-wall") {
                suppressWall = true;
                continue;
            }
            if (arg == "-f" || arg == "--force") {
                force = true;
                continue;
            }
            if (arg == "-w" || arg == "--wtmp") {
                wtmpOnly = true;
                continue;
            }

            std::cerr << "reboot: unrecognized option '" << arg << "'\n";
            PrintUsage();
            return 2;
        }
        return -1;
    }

    void PrintUsage() const {
        std::cout << R"(reboot(1)               CrossShell for UNIX Reference Manual                reboot(1)

    NAME
        reboot - reboot, halt, or power off the Windows system

    SYNOPSIS
        reboot [OPTIONS]

    DESCRIPTION
        reboot initiates a system reboot using Windows shutdown privileges
        (SeShutdownPrivilege) and logging reasons.

    OPTIONS
        -f, --force
            Force immediate reboot without prompting or notifying users.

        -p, --poweroff
            Power down the machine instead of rebooting.

        -w, --wtmp-only
            Write reboot record to system logs without actually rebooting.

        -n, --no-wall
            Do not broadcast wall message to connected users.

        -h, --help
            Display this reference manual.

        --version
            Output version information and exit.

    EXAMPLES
        reboot
            Reboot local machine gracefully.

        reboot -f
            Force immediate system reboot.

    CrossShell for UNIX                                                 reboot(1)
)";
    }

    void PrintVersion() const {
        std::cout << "reboot v1.0.0\n";
    }
};

// ============================================================================
// 3. REBOOT ENGINE
// ============================================================================

class RebootEngine {
public:
    static std::string ToUpper(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), [](unsigned char ch) {
            return static_cast<char>(std::toupper(ch));
        });
        return str;
    }

    static int Execute(const RebootOptions& options) {
        SetConsoleTitleA("System Reboot");

        if (!options.suppressWall) {
            std::cout << "\n WARNING: You are about to reboot this System.\n\n";
        }

        std::cout << "Are you sure you want to proceed? (Y/N): ";
        std::string input;
        std::cin >> input;

        input = ToUpper(input);

        if (input == "Y" || input == "YES") {
            if (!PrivilegeManager::IsRunningAsAdmin() && !options.elevatedRun) {
                std::cout << "\nAdministrator rights are required to reboot this system." << std::endl;
                std::cout << "Showing UAC prompt for admin approval..." << std::endl;

                if (ElevationManager::RelaunchElevated()) {
                    std::cout << "Elevated instance launched." << std::endl;
                } else {
                    std::cerr << "Unable to request administrator privileges. Error code: " << GetLastError() << std::endl;
                }

                return 0;
            }

            if (options.wtmpOnly) {
                std::cout << "wtmp record update requested; no reboot performed." << std::endl;
                return 0;
            }

            std::cout << "\nAttempting to reboot the system..." << std::endl;

            if (PrivilegeManager::EnableShutdownPrivilege()) {
                UINT flags = EWX_REBOOT;
                if (options.force) flags |= EWX_FORCEIFHUNG;
                if (ExitWindowsEx(flags, SHTDN_REASON_MAJOR_OTHER | SHTDN_REASON_MINOR_OTHER)) {
                    return 0;
                } else {
                    std::cerr << "ExitWindowsEx failed. Error code: " << GetLastError() << std::endl;
                    return 1;
                }
            } else {
                std::cerr << "Failed to acquire necessary reboot privileges." << std::endl;
                return 1;
            }
        } else {
            std::cout << "\nReboot canceled." << std::endl;
            std::cout << "Press Enter to exit...";
            std::cin.ignore(10000, '\n');
            std::cin.get();
        }

        return 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class RebootApplication {
public:
    int Run(int argc, char* argv[]) const {
        RebootOptions options;
        int parseResult = options.Parse(argc, argv);
        if (parseResult >= 0) {
            if (options.showHelp) {
                options.PrintUsage();
                return 0;
            }
            if (options.showVersion) {
                options.PrintVersion();
                return 0;
            }
            return parseResult;
        }

        return RebootEngine::Execute(options);
    }
};

int main(int argc, char* argv[]) {
    RebootApplication app;
    return app.Run(argc, argv);
}
