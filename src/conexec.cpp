/*
BSD 3 Clause License
--------------------

CrossShell for UNIX
Copyright (c) 2026, Roberto J Dohnert
All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list of conditions, and the following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions, and the following disclaimer
in the documentation and/or other materials provided with the distribution. Neither the name of [project] nor the names of its
contributors may be used to endorse or promote products derived from this software without specific prior written permission.

Disclaimer:
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAG
*/

#define WIN32_LEAN_AND_MEAN
#define SECURITY_WIN32
#include <windows.h>
#include <sddl.h>
#include <userenv.h>
#include <aclapi.h>
#include <winioctl.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <filesystem>
#include <chrono>
#include <memory>
#include <atomic>
#include <mutex>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "userenv.lib")

namespace fs = std::filesystem;

#define RUNCON_VERSION L"3.0.0"
#define RUNCON_AUTHOR  L"Roberto J Dohnert"

// --- Global State for Signal Cleanup ---
namespace GlobalState {
    std::atomic<bool> isTerminating{ false };
    std::atomic<bool> signalCleanupStarted{ false };
    std::mutex cleanupMutex;
    fs::path activeWorkspace;
    HANDLE activeJob = nullptr;
    std::wstring appContainerProfileName;
}

enum class IsolationLevel {
    TotalLockdown = -1,  // AppContainer 0-network, strict Job limits, ephemeral scratch disk
    EphemeralNet  = -2,  // AppContainer network allowed, transient storage wiped on exit
    PromptCommit  = -3,  // AppContainer network allowed, staged sandbox with interactive review
    TempWorkspace = -4,  // AppContainer network allowed, output saved to dedicated temp folder
    Normal        = -5   // Current user standard execution
};

struct RunconConfig {
    IsolationLevel level = IsolationLevel::Normal;
    std::wstring command;
    std::wstring workingDirectory;
    bool stageCurrentDir = false;
    bool verbose = false;
};

struct FileSnapshot {
    fs::file_time_type lastWrite;
    uintmax_t size;
};

// --- RAII Resource Wrappers ---
struct HandleCloser {
    void operator()(HANDLE h) const noexcept {
        if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
    }
};
using ScopedHandle = std::unique_ptr<void, HandleCloser>;

static void DrainJobObject(HANDLE hJob, DWORD timeoutMs = 2000) {
    if (!hJob) return;

    const auto start = std::chrono::steady_clock::now();
    while (true) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info = { 0 };
        if (!QueryInformationJobObject(
                hJob,
                JobObjectBasicAccountingInformation,
                &info,
                sizeof(info),
                nullptr) || info.ActiveProcesses == 0) {
            break;
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeoutMs) {
            break;
        }
        Sleep(15);
    }
}

// --- UI, Version & Help System ---
void PrintVersion() {
    std::wcout << L"conexec version " << RUNCON_VERSION << L" (Windows x86_64)\n";
    std::wcout << L"Copyright (C) " << RUNCON_AUTHOR << L". All rights reserved.\n";
    std::wcout << L"Enforces Mandatory Integrity Control (MIC), AppContainer isolation, and Job limits.\n";
}

void PrintHeader() {
    std::wcout << L"\033[96m============================================================\033[0m\n";
    std::wcout << L"\033[96m  conexec [v" << RUNCON_VERSION << L"] - Security Isolation Engine      \033[0m\n";
    std::wcout << L"\033[96m  Copyright (C) " << RUNCON_AUTHOR << L"              \033[0m\n";
    std::wcout << L"\033[96m============================================================\033[0m\n";
}

void PrintHelp() {
    std::wcout << LR"(conexec(1)              CrossShell for UNIX Reference Manual                  conexec(1)

    NAME
        conexec - execute a command under a Windows isolation policy

    SYNOPSIS
        conexec LEVEL [OPTIONS] -- COMMAND [ARGS...]

    DESCRIPTION
        Launches a process with Windows Job Object, integrity-level, token, and
        AppContainer isolation controls. Child processes inherit standard handles.

    SECURITY LEVELS
        -1
            Total lockdown: AppContainer/WFP network blocking, ephemeral disk, 512 MiB limit.

        -2
            Ephemeral network-enabled sandbox; changes purged, 2 GiB memory limit.

        -3
            Interactive staged sandbox requiring review before merging output.

        -4
            Low-integrity temporary workspace with output preserved.

        -5
            Normal execution under the current user.

    OPTIONS
        -s, --stage
            Copy the current directory into the sandbox.

        -d, --dir PATH
            Set the target working directory.

        -v, --verbose
            Display detailed execution diagnostics.

        -V, --version
            Display version and license information.

        -h, --help
            Display this reference manual.

        --
            End options and introduce COMMAND.

    EXAMPLES
        conexec -1 -- untrusted_installer.exe /S
            Run installer in zero-trust total lockdown sandbox.

        conexec -2 -- python test_scraper.py
            Run scraper in ephemeral network-enabled sandbox.

        conexec -3 -s -- clang-format -i main.cpp
            Run formatter in interactive staged workspace.

        conexec -4 -- npm install
            Run build in low-integrity workspace.

    CrossShell for UNIX                                                    conexec(1)
)";
}

// --- Helper functions for Safe Directory Cleanup ---
static void ClearReadOnlyAttributes(const fs::path& path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) return;

    if (fs::is_directory(path, ec)) {
        for (const auto& entry : fs::recursive_directory_iterator(path, ec)) {
            std::error_code localEc;
            auto entryPath = entry.path();
            DWORD attrs = GetFileAttributesW(entryPath.c_str());
            if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
                SetFileAttributesW(entryPath.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
            }
        }
    }

    DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
        SetFileAttributesW(path.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
    }
}

static void SafeRemoveAll(const fs::path& path) {
    if (path.empty()) return;
    std::error_code ec;
    ClearReadOnlyAttributes(path);
    fs::remove_all(path, ec);
}

// --- Helper function for robust command line argument escaping ---
static std::wstring EscapeArgument(const std::wstring& arg) {
    if (arg.empty()) {
        return L"\"\"";
    }
    bool needQuote = false;
    for (wchar_t c : arg) {
        if (c == L' ' || c == L'\t' || c == L'\n' || c == L'\v' || c == L'\"') {
            needQuote = true;
            break;
        }
    }
    if (!needQuote) {
        return arg;
    }

    std::wstring result = L"\"";
    for (size_t i = 0; i < arg.length(); ++i) {
        size_t backslashes = 0;
        while (i < arg.length() && arg[i] == L'\\') {
            backslashes++;
            i++;
        }

        if (i == arg.length()) {
            result.append(backslashes * 2, L'\\');
        } else if (arg[i] == L'\"') {
            result.append(backslashes * 2 + 1, L'\\');
            result.push_back(L'\"');
        } else {
            result.append(backslashes, L'\\');
            result.push_back(arg[i]);
        }
    }
    result.push_back(L'\"');
    return result;
}

#ifndef IO_REPARSE_TAG_APPEXECLINK
#define IO_REPARSE_TAG_APPEXECLINK (0x8000001B)
#endif

typedef struct _REPARSE_DATA_BUFFER {
    ULONG  ReparseTag;
    USHORT ReparseDataLength;
    USHORT Reserved;
    union {
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            ULONG  Flags;
            WCHAR  PathBuffer[1];
        } SymbolicLinkReparseBuffer;
        struct {
            USHORT SubstituteNameOffset;
            USHORT SubstituteNameLength;
            USHORT PrintNameOffset;
            USHORT PrintNameLength;
            WCHAR  PathBuffer[1];
        } MountPointReparseBuffer;
        struct {
            UCHAR  DataBuffer[1];
        } GenericReparseBuffer;
    } DUMMYUNIONNAME;
} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

static std::wstring ResolveAppExecutionAlias(const std::wstring& aliasPath) {
    HANDLE hFile = CreateFileW(
        aliasPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS,
        nullptr
    );

    if (hFile == INVALID_HANDLE_VALUE) {
        return L"";
    }

    std::vector<BYTE> buffer(16384);
    DWORD bytesReturned = 0;
    BOOL result = DeviceIoControl(
        hFile,
        FSCTL_GET_REPARSE_POINT,
        nullptr,
        0,
        buffer.data(),
        static_cast<DWORD>(buffer.size()),
        &bytesReturned,
        nullptr
    );

    CloseHandle(hFile);

    if (!result) {
        return L"";
    }

    PREPARSE_DATA_BUFFER reparseData = reinterpret_cast<PREPARSE_DATA_BUFFER>(buffer.data());
    if (reparseData->ReparseTag != IO_REPARSE_TAG_APPEXECLINK) {
        return L"";
    }

    BYTE* dataStart = reparseData->GenericReparseBuffer.DataBuffer;
    wchar_t* pStr = reinterpret_cast<wchar_t*>(dataStart + 4); // skip 4 bytes version

    // String 1: Package Family Name
    size_t len1 = wcslen(pStr);
    pStr += len1 + 1;

    // String 2: Package Relative Application ID
    size_t len2 = wcslen(pStr);
    pStr += len2 + 1;

    // String 3: Target Path
    return std::wstring(pStr);
}

static std::wstring ResolveExecutableInCommand(const std::wstring& command) {
    if (command.empty()) return command;

    std::wstring rawExe;
    std::wstring rest;
    if (command[0] == L'"') {
        size_t endQuote = command.find(L'"', 1);
        if (endQuote != std::wstring::npos) {
            rawExe = command.substr(1, endQuote - 1);
            rest = command.substr(endQuote + 1);
        } else {
            rawExe = command.substr(1);
        }
    } else {
        size_t pos = command.find(L' ');
        while (pos != std::wstring::npos) {
            std::wstring prefix = command.substr(0, pos);
            if (fs::exists(prefix)) {
                rawExe = prefix;
                rest = command.substr(pos);
                break;
            }
            wchar_t resolved[MAX_PATH] = { 0 };
            if (SearchPathW(nullptr, prefix.c_str(), L".exe", MAX_PATH, resolved, nullptr) > 0) {
                rawExe = prefix;
                rest = command.substr(pos);
                break;
            }
            pos = command.find(L' ', pos + 1);
        }
        if (rawExe.empty()) {
            pos = command.find(L' ');
            if (pos != std::wstring::npos) {
                rawExe = command.substr(0, pos);
                rest = command.substr(pos);
            } else {
                rawExe = command;
            }
        }
    }

    if (rawExe.empty()) return command;

    std::wstring resolvedPath = rawExe;
    wchar_t resolved[MAX_PATH] = { 0 };
    if (SearchPathW(nullptr, rawExe.c_str(), L".exe", MAX_PATH, resolved, nullptr) > 0) {
        resolvedPath = resolved;
    } else if (fs::exists(rawExe)) {
        std::error_code ec;
        resolvedPath = fs::absolute(rawExe, ec).wstring();
    }

    std::wstring aliasTarget = ResolveAppExecutionAlias(resolvedPath);
    if (!aliasTarget.empty()) {
        resolvedPath = aliasTarget;
    }

    return EscapeArgument(resolvedPath) + rest;
}

// --- Console Control Handler (Ctrl+C / Break) ---
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    switch (ctrlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT: {
        if (GlobalState::signalCleanupStarted.exchange(true)) {
            return TRUE;
        }

        GlobalState::isTerminating = true;
        std::lock_guard<std::mutex> lock(GlobalState::cleanupMutex);
        if (GlobalState::activeJob) {
            TerminateJobObject(GlobalState::activeJob, 1);
            DrainJobObject(GlobalState::activeJob, 1500);
            GlobalState::activeJob = nullptr;
        }
        if (!GlobalState::activeWorkspace.empty()) {
            SafeRemoveAll(GlobalState::activeWorkspace);
            GlobalState::activeWorkspace.clear();
        }
        if (!GlobalState::appContainerProfileName.empty()) {
            DeleteAppContainerProfile(GlobalState::appContainerProfileName.c_str());
            GlobalState::appContainerProfileName.clear();
        }
        ExitProcess(1);
        return TRUE;
    }
    default:
        return FALSE;
    }
}

// --- Windows Security & ACL Management ---
class SecurityEngine {
public:
    static bool GrantAppContainerAccess(const fs::path& targetPath, DWORD permissions) {
        PSID pSidPackages = nullptr;
        if (!ConvertStringSidToSidW(L"S-1-15-2-1", &pSidPackages)) {
            return false;
        }

        EXPLICIT_ACCESSW ea = { 0 };
        ea.grfAccessPermissions = permissions;
        ea.grfAccessMode = GRANT_ACCESS;
        ea.grfInheritance = CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE;
        ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        ea.Trustee.TrusteeType = TRUSTEE_IS_GROUP;
        ea.Trustee.ptstrName = (LPWSTR)pSidPackages;

        PACL pOldAcl = nullptr;
        PSECURITY_DESCRIPTOR pSD = nullptr;
        GetNamedSecurityInfoW(targetPath.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &pOldAcl, nullptr, &pSD);

        PACL pNewAcl = nullptr;
        DWORD res = SetEntriesInAclW(1, &ea, pOldAcl, &pNewAcl);
        if (res == ERROR_SUCCESS) {
            SetNamedSecurityInfoW((LPWSTR)targetPath.c_str(), SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, pNewAcl, nullptr);
            LocalFree(pNewAcl);
        }

        if (pSD) LocalFree(pSD);
        LocalFree(pSidPackages);
        return (res == ERROR_SUCCESS);
    }

    static void EnsureBinaryReadable(const std::wstring& command) {
        std::wstring rawExe;
        if (command.empty()) return;

        if (command[0] == L'"') {
            size_t endQuote = command.find(L'"', 1);
            if (endQuote != std::wstring::npos) {
                rawExe = command.substr(1, endQuote - 1);
            }
        } else {
            size_t pos = command.find(L' ');
            while (pos != std::wstring::npos) {
                std::wstring prefix = command.substr(0, pos);
                if (fs::exists(prefix)) {
                    rawExe = prefix;
                    break;
                }
                wchar_t resolved[MAX_PATH] = { 0 };
                if (SearchPathW(nullptr, prefix.c_str(), L".exe", MAX_PATH, resolved, nullptr) > 0) {
                    rawExe = prefix;
                    break;
                }
                pos = command.find(L' ', pos + 1);
            }
            if (rawExe.empty()) {
                pos = command.find(L' ');
                rawExe = (pos != std::wstring::npos) ? command.substr(0, pos) : command;
            }
        }

        if (rawExe.empty()) return;

        // Resolve absolute path or search PATH environment
        wchar_t resolvedPath[MAX_PATH] = { 0 };
        if (SearchPathW(nullptr, rawExe.c_str(), L".exe", MAX_PATH, resolvedPath, nullptr) > 0) {
            GrantAppContainerAccess(resolvedPath, GENERIC_READ | GENERIC_EXECUTE);
        } else if (fs::exists(rawExe)) {
            GrantAppContainerAccess(rawExe, GENERIC_READ | GENERIC_EXECUTE);
        }
    }

    static std::vector<wchar_t> CreateSandboxedEnvironmentBlock(const fs::path& workspacePath) {
        LPWCH pEnvStrings = GetEnvironmentStringsW();
        if (!pEnvStrings) return {};

        std::vector<std::wstring> envVars;
        LPWCH current = pEnvStrings;
        while (*current) {
            std::wstring var(current);
            // Filter out existing temp and appdata variables
            if (var.rfind(L"TEMP=", 0) != 0 &&
                var.rfind(L"TMP=", 0) != 0 &&
                var.rfind(L"LOCALAPPDATA=", 0) != 0) {
                envVars.push_back(var);
            }
            current += wcslen(current) + 1;
        }
        FreeEnvironmentStringsW(pEnvStrings);

        // Redirect TEMP, TMP, and LOCALAPPDATA directly to the sandboxed workspace
        envVars.push_back(L"TEMP=" + workspacePath.wstring());
        envVars.push_back(L"TMP=" + workspacePath.wstring());
        envVars.push_back(L"LOCALAPPDATA=" + workspacePath.wstring());

        // Serialize environment block (double null terminated)
        std::vector<wchar_t> envBlock;
        for (const auto& var : envVars) {
            envBlock.insert(envBlock.end(), var.begin(), var.end());
            envBlock.push_back(L'\0');
        }
        envBlock.push_back(L'\0');

        return envBlock;
    }

    static HANDLE CreateJobSandbox(IsolationLevel level) {
        if (level == IsolationLevel::Normal) {
            return nullptr;
        }

        HANDLE hJob = CreateJobObjectW(nullptr, nullptr);
        if (!hJob) return nullptr;

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE | 
                                                JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;

        if (level == IsolationLevel::TotalLockdown || level == IsolationLevel::EphemeralNet) {
            jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
            jeli.BasicLimitInformation.ActiveProcessLimit = (level == IsolationLevel::TotalLockdown) ? 4 : 16;

            jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_JOB_MEMORY;
            jeli.JobMemoryLimit = (level == IsolationLevel::TotalLockdown)
                ? static_cast<SIZE_T>(512ULL * 1024ULL * 1024ULL)
                : static_cast<SIZE_T>(2ULL * 1024ULL * 1024ULL * 1024ULL);
        }

        SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));

        if (level == IsolationLevel::TotalLockdown || level == IsolationLevel::EphemeralNet) {
            JOBOBJECT_BASIC_UI_RESTRICTIONS ui = { 0 };
            ui.UIRestrictionsClass = JOB_OBJECT_UILIMIT_HANDLES |
                                     JOB_OBJECT_UILIMIT_READCLIPBOARD |
                                     JOB_OBJECT_UILIMIT_WRITECLIPBOARD |
                                     JOB_OBJECT_UILIMIT_SYSTEMPARAMETERS |
                                     JOB_OBJECT_UILIMIT_DESKTOP |
                                     JOB_OBJECT_UILIMIT_EXITWINDOWS;
            SetInformationJobObject(hJob, JobObjectBasicUIRestrictions, &ui, sizeof(ui));
        }

        return hJob;
    }
};

// --- Execution Dispatcher ---
class ExecutionDispatcher {
public:
    static int Execute(const RunconConfig& cfg) {
        RunconConfig localCfg = cfg;
        localCfg.command = ResolveExecutableInCommand(cfg.command);

        fs::path destDir = localCfg.workingDirectory.empty() ? fs::current_path() : fs::path(localCfg.workingDirectory);
        fs::path targetDir = destDir;
        fs::path tempWorkspace;
        std::map<fs::path, FileSnapshot> initialSnapshot;
        std::vector<wchar_t> sandboxedEnv;

        if (localCfg.level != IsolationLevel::Normal) {
            auto epoch = std::chrono::steady_clock::now().time_since_epoch().count();
            tempWorkspace = fs::temp_directory_path() / (L"runcon_ws_" + std::to_wstring(epoch));
            fs::create_directories(tempWorkspace);
            
            {
                std::lock_guard<std::mutex> lock(GlobalState::cleanupMutex);
                GlobalState::activeWorkspace = tempWorkspace;
            }
            
            targetDir = tempWorkspace;
            SecurityEngine::GrantAppContainerAccess(tempWorkspace, GENERIC_ALL);
            sandboxedEnv = SecurityEngine::CreateSandboxedEnvironmentBlock(tempWorkspace);

            if (localCfg.stageCurrentDir) {
                if (localCfg.verbose) std::wcout << L"\033[33m[*] Staging workspace directory into sandbox...\033[0m\n";
                for (const auto& entry : fs::directory_iterator(destDir)) {
                    if (entry.path() != tempWorkspace) {
                        std::error_code ec;
                        fs::path destination = tempWorkspace / entry.path().filename();
                        fs::copy(entry.path(), destination, fs::copy_options::recursive | fs::copy_options::skip_existing, ec);
                    }
                }
            }

            if (localCfg.level == IsolationLevel::PromptCommit) {
                std::error_code ec;
                for (const auto& entry : fs::recursive_directory_iterator(tempWorkspace, ec)) {
                    if (fs::is_regular_file(entry.status())) {
                        fs::path rel = fs::relative(entry.path(), tempWorkspace);
                        initialSnapshot[rel] = { fs::last_write_time(entry.path(), ec), fs::file_size(entry.path(), ec) };
                    }
                }
            }
        }

        ScopedHandle hJob(SecurityEngine::CreateJobSandbox(localCfg.level));
        {
            std::lock_guard<std::mutex> lock(GlobalState::cleanupMutex);
            GlobalState::activeJob = hJob.get();
        }

        if (localCfg.level != IsolationLevel::Normal) {
            SecurityEngine::EnsureBinaryReadable(localCfg.command);
        }

        PSID appContainerSid = nullptr;
        std::wstring acProfileName;
        std::vector<SID_AND_ATTRIBUTES> capabilities;
        PSID pNetClientSid = nullptr;
        PSID pNetClientServerSid = nullptr;
        PSID pPrivateNetworkClientServerSid = nullptr;

        if (localCfg.level != IsolationLevel::Normal) {
            acProfileName = L"runcon_ac_" + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
            {
                std::lock_guard<std::mutex> lock(GlobalState::cleanupMutex);
                GlobalState::appContainerProfileName = acProfileName;
            }

            HRESULT hr = CreateAppContainerProfile(acProfileName.c_str(), acProfileName.c_str(), acProfileName.c_str(), nullptr, 0, &appContainerSid);
            if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
                DeriveAppContainerSidFromAppContainerName(acProfileName.c_str(), &appContainerSid);
            }

            // Strict Fail-Closed Security Policy
            if (!appContainerSid) {
                std::wcerr << L"\033[31m[-] Fatal: Failed to initialize AppContainer security context (HRESULT: " 
                           << std::hex << hr << L"). Execution aborted.\033[0m\n";
                SafeRemoveAll(tempWorkspace);
                return 1;
            }

            if (localCfg.level != IsolationLevel::TotalLockdown) {
                if (ConvertStringSidToSidW(L"S-1-15-3-1", &pNetClientSid)) { // internetClient capability
                    SID_AND_ATTRIBUTES sa = { 0 };
                    sa.Sid = pNetClientSid;
                    sa.Attributes = SE_GROUP_ENABLED;
                    capabilities.push_back(sa);
                }
                if (ConvertStringSidToSidW(L"S-1-15-3-2", &pNetClientServerSid)) { // internetClientServer capability
                    SID_AND_ATTRIBUTES sa = { 0 };
                    sa.Sid = pNetClientServerSid;
                    sa.Attributes = SE_GROUP_ENABLED;
                    capabilities.push_back(sa);
                }
                if (ConvertStringSidToSidW(L"S-1-15-3-3", &pPrivateNetworkClientServerSid)) { // privateNetworkClientServer capability
                    SID_AND_ATTRIBUTES sa = { 0 };
                    sa.Sid = pPrivateNetworkClientServerSid;
                    sa.Attributes = SE_GROUP_ENABLED;
                    capabilities.push_back(sa);
                }
            }

            if (localCfg.verbose) {
                if (localCfg.level == IsolationLevel::TotalLockdown) {
                    std::wcout << L"\033[32m[+] Network Lockdown: Zero-Capability AppContainer active (WFP socket blocking).\033[0m\n";
                } else {
                    std::wcout << L"\033[32m[+] AppContainer active with internetClient capability.\033[0m\n";
                }
            }
        }

        // Validate Standard Handles
        std::vector<HANDLE> validInheritHandles;
        for (DWORD stdId : { STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE }) {
            HANDLE h = GetStdHandle(stdId);
            if (h && h != INVALID_HANDLE_VALUE) {
                SetHandleInformation(h, HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT);
                validInheritHandles.push_back(h);
            }
        }

        DWORD attrCount = 0;
        if (appContainerSid) attrCount++;
        if (!validInheritHandles.empty()) attrCount++;

        SIZE_T attrSize = 0;
        if (attrCount > 0) {
            InitializeProcThreadAttributeList(nullptr, attrCount, 0, &attrSize);
        }

        std::vector<BYTE> attrBuffer(attrSize);
        PPROC_THREAD_ATTRIBUTE_LIST pAttrList = attrSize ? reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(attrBuffer.data()) : nullptr;
        SECURITY_CAPABILITIES secCaps = { 0 };

        if (pAttrList) {
            InitializeProcThreadAttributeList(pAttrList, attrCount, 0, &attrSize);

            if (!validInheritHandles.empty()) {
                UpdateProcThreadAttribute(pAttrList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, validInheritHandles.data(), 
                                           validInheritHandles.size() * sizeof(HANDLE), nullptr, nullptr);
            }

            if (appContainerSid) {
                secCaps.AppContainerSid = appContainerSid;
                secCaps.Capabilities = capabilities.empty() ? nullptr : capabilities.data();
                secCaps.CapabilityCount = static_cast<DWORD>(capabilities.size());
                UpdateProcThreadAttribute(pAttrList, 0, PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES, &secCaps, sizeof(secCaps), nullptr, nullptr);
            }
        }

        STARTUPINFOEXW siEx = { 0 };
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
        siEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
        siEx.StartupInfo.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
        siEx.StartupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
        siEx.StartupInfo.hStdError  = GetStdHandle(STD_ERROR_HANDLE);
        siEx.lpAttributeList = pAttrList;

        PROCESS_INFORMATION pi = { 0 };
        std::vector<wchar_t> cmdLine(localCfg.command.begin(), localCfg.command.end());
        cmdLine.push_back(L'\0');

        DWORD creationFlags = CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT | (pAttrList ? EXTENDED_STARTUPINFO_PRESENT : 0);
        LPVOID pEnvironment = sandboxedEnv.empty() ? nullptr : sandboxedEnv.data();

        if (localCfg.verbose) {
            std::wcout << L"[*] Target Command: " << localCfg.command << L"\n";
            std::wcout << L"[*] Active Sandbox Directory: " << targetDir.wstring() << L"\n";
        }

        BOOL bCreated = CreateProcessW(
            nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
            creationFlags, pEnvironment, targetDir.c_str(), &siEx.StartupInfo, &pi
        );

        if (!bCreated) {
            DWORD err = GetLastError();
            std::wcerr << L"\033[31m[-] Execution failed with Win32 Error: " << err << L"\033[0m\n";
            if (pNetClientSid) LocalFree(pNetClientSid);
            if (pNetClientServerSid) LocalFree(pNetClientServerSid);
            if (pPrivateNetworkClientServerSid) LocalFree(pPrivateNetworkClientServerSid);
            if (appContainerSid) {
                FreeSid(appContainerSid);
                DeleteAppContainerProfile(acProfileName.c_str());
            }
            if (pAttrList) DeleteProcThreadAttributeList(pAttrList);
            SafeRemoveAll(tempWorkspace);
            
            std::lock_guard<std::mutex> lock(GlobalState::cleanupMutex);
            GlobalState::activeJob = nullptr;
            GlobalState::appContainerProfileName.clear();
            return 1;
        }

        ScopedHandle hProc(pi.hProcess);
        ScopedHandle hThrd(pi.hThread);

        if (hJob.get() && !AssignProcessToJobObject(hJob.get(), pi.hProcess)) {
            std::wcerr << L"\033[31m[-] Failed to assign process to the sandbox job.\033[0m\n";
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, INFINITE);
            if (pAttrList) DeleteProcThreadAttributeList(pAttrList);
            if (pNetClientSid) LocalFree(pNetClientSid);
            if (pNetClientServerSid) LocalFree(pNetClientServerSid);
            if (pPrivateNetworkClientServerSid) LocalFree(pPrivateNetworkClientServerSid);
            if (appContainerSid) {
                FreeSid(appContainerSid);
                DeleteAppContainerProfile(acProfileName.c_str());
            }
            SafeRemoveAll(tempWorkspace);
            return 1;
        }

        // Resume process execution
        ResumeThread(pi.hThread);

        // Await process termination
        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode = 0;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        // The direct process may have spawned descendants. Drain the job before
        // deleting the AppContainer profile or its temporary workspace.
        if (hJob.get()) {
            TerminateJobObject(hJob.get(), exitCode);
            DrainJobObject(hJob.get(), 2000);
        }

        // Cleanup AppContainer profile and capabilities
        if (pNetClientSid) LocalFree(pNetClientSid);
        if (pNetClientServerSid) LocalFree(pNetClientServerSid);
        if (pPrivateNetworkClientServerSid) LocalFree(pPrivateNetworkClientServerSid);
        if (appContainerSid) {
            FreeSid(appContainerSid);
            DeleteAppContainerProfile(acProfileName.c_str());
        }
        if (pAttrList) {
            DeleteProcThreadAttributeList(pAttrList);
        }
        
        {
            std::lock_guard<std::mutex> lock(GlobalState::cleanupMutex);
            GlobalState::activeJob = nullptr;
            GlobalState::appContainerProfileName.clear();
        }

        std::wcout << L"\n\033[36m[*] Process terminated with exit code: \033[0m" << exitCode << L"\n";

        // Post-execution output policy handling
        ProcessOutputDispositions(localCfg, tempWorkspace, destDir, initialSnapshot);

        return static_cast<int>(exitCode);
    }

private:
    static void ProcessOutputDispositions(
        const RunconConfig& cfg,
        const fs::path& stageDir,
        const fs::path& destDir,
        const std::map<fs::path, FileSnapshot>& initialSnapshot) 
    {
        if (stageDir.empty() || !fs::exists(stageDir)) return;

        std::vector<fs::path> changedFiles;
        std::error_code ec;

        for (const auto& entry : fs::recursive_directory_iterator(stageDir, ec)) {
            if (fs::is_regular_file(entry.status())) {
                fs::path rel = fs::relative(entry.path(), stageDir);
                auto it = initialSnapshot.find(rel);
                if (it == initialSnapshot.end()) {
                    changedFiles.push_back(rel);
                } else {
                    if (fs::last_write_time(entry.path(), ec) != it->second.lastWrite ||
                        fs::file_size(entry.path(), ec) != it->second.size) {
                        changedFiles.push_back(rel);
                    }
                }
            }
        }

        switch (cfg.level) {
        case IsolationLevel::TotalLockdown:
        case IsolationLevel::EphemeralNet: {
            std::wcout << L"\033[33m[*] Policy Action: Discarded " << changedFiles.size() << L" transient modification(s).\033[0m\n";
            SafeRemoveAll(stageDir);
            break;
        }

        case IsolationLevel::TempWorkspace: {
            std::wcout << L"\033[32m[*] Level -4 Policy: Sandboxed output preserved in:\033[0m\n    " 
                       << stageDir.wstring() << L"\n";
            break;
        }

        case IsolationLevel::PromptCommit: {
            if (changedFiles.empty()) {
                std::wcout << L"[*] No file modifications or outputs detected.\n";
                SafeRemoveAll(stageDir);
                break;
            }

            std::wcout << L"\n\033[93m[?] Staged Changes Summary (" << changedFiles.size() << L" file(s) modified/created):\033[0m\n";
            for (const auto& rel : changedFiles) {
                fs::path fullPath = stageDir / rel;
                std::wcout << L"    [+] " << rel.wstring() 
                           << L" (" << fs::file_size(fullPath, ec) << L" bytes)\n";
            }

            std::wcout << L"\n\033[93m[?] Commit staged modifications back to " << destDir.wstring() << L"? [y/N]: \033[0m";
            std::wstring choice;
            std::wcin >> choice;

            if (choice == L"y" || choice == L"Y") {
                for (const auto& rel : changedFiles) {
                    fs::path source = stageDir / rel;
                    fs::path target = destDir / rel;
                    fs::create_directories(target.parent_path(), ec);
                    fs::copy_file(source, target, fs::copy_options::overwrite_existing, ec);
                }
                std::wcout << L"\033[32m[+] Staged changes merged successfully.\033[0m\n";
            } else {
                std::wcout << L"\033[31m[-] Discarding all sandboxed modifications.\033[0m\n";
            }
            SafeRemoveAll(stageDir);
            break;
        }

        default:
            break;
        }
    }
};

// --- CLI Parsing & Main Entry ---
int wmain(int argc, wchar_t* argv[]) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    if (argc < 2) {
        PrintHelp();
        return 0;
    }

    RunconConfig cfg;
    bool parsingCommand = false;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (parsingCommand) {
            if (!cfg.command.empty()) cfg.command += L" ";
            cfg.command += EscapeArgument(arg);
            continue;
        }

        if (arg == L"-h" || arg == L"--help") {
            PrintHelp();
            return 0;
        } else if (arg == L"-V" || arg == L"--version") {
            PrintVersion();
            return 0;
        } else if (arg == L"-5") {
            cfg.level = IsolationLevel::Normal;
        } else if (arg == L"-4") {
            cfg.level = IsolationLevel::TempWorkspace;
        } else if (arg == L"-3") {
            cfg.level = IsolationLevel::PromptCommit;
        } else if (arg == L"-2") {
            cfg.level = IsolationLevel::EphemeralNet;
        } else if (arg == L"-1") {
            cfg.level = IsolationLevel::TotalLockdown;
        } else if (arg == L"-s" || arg == L"--stage") {
            cfg.stageCurrentDir = true;
        } else if (arg == L"-v" || arg == L"--verbose") {
            cfg.verbose = true;
        } else if ((arg == L"-d" || arg == L"--dir") && (i + 1 < argc)) {
            cfg.workingDirectory = argv[++i];
        } else if (arg == L"--") {
            parsingCommand = true;
        } else {
            parsingCommand = true;
            cfg.command += EscapeArgument(arg);
        }
    }

    if (cfg.command.empty()) {
        std::wcerr << L"\033[31m[-] Error: No execution target specified.\033[0m\n";
        std::wcerr << L"    Run 'conexec --help' for syntax.\n";
        return 1;
    }

    return ExecutionDispatcher::Execute(cfg);
}