#include "handle_closer.h"

bool DrainJobObject(HANDLE hJob, DWORD timeoutMs) {
    if (!hJob) return true;

    const auto start = std::chrono::steady_clock::now();
    while (true) {
        JOBOBJECT_BASIC_ACCOUNTING_INFORMATION info = { 0 };
        if (!QueryInformationJobObject(
                hJob,
                JobObjectBasicAccountingInformation,
                &info,
                sizeof(info),
                nullptr)) {
            return false; // query failed
        }
        if (info.ActiveProcesses == 0) {
            return true; // fully drained
        }

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeoutMs) {
            return false; // timed out with processes still active
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
void ClearReadOnlyAttributes(const fs::path& path) {
    std::error_code ec;
    if (!fs::exists(path, ec)) return;

    // Do not follow directory symlinks/junctions when clearing attributes.
    if (fs::is_directory(path, ec) && !fs::is_symlink(path, ec)) {
        for (const auto& entry : fs::recursive_directory_iterator(
                 path, fs::directory_options::skip_permission_denied, ec)) {
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

void SafeRemoveAll(const fs::path& path) {
    if (path.empty()) return;
    std::error_code ec;

    // Reparse-point policy: a symlink/junction is unlinked itself, never
    // followed, so recursive deletion cannot escape into a target outside the
    // sandbox tree.
    if (fs::is_symlink(path, ec)) {
        fs::remove(path, ec);
        return;
    }

    ClearReadOnlyAttributes(path);
    fs::remove_all(path, ec);
}

// --- Helper function for robust command line argument escaping ---
std::wstring EscapeArgument(const std::wstring& arg) {
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

// Read a NUL-terminated wide string from within a bounded buffer region.
// Returns true and sets `out` if a terminator is found within `remaining`
// bytes; otherwise returns false (malformed / truncated buffer).
static bool ReadBoundedWideString(const BYTE* cursor, size_t remaining, std::wstring& out, size_t& consumedBytes) {
    // Scan for a NUL wchar within the remaining bytes.
    size_t maxChars = remaining / sizeof(wchar_t);
    const wchar_t* wcur = reinterpret_cast<const wchar_t*>(cursor);
    size_t len = 0;
    bool found = false;
    for (; len < maxChars; ++len) {
        if (wcur[len] == L'\0') { found = true; break; }
    }
    if (!found) return false;
    out.assign(wcur, len);
    consumedBytes = (len + 1) * sizeof(wchar_t); // include the NUL
    return true;
}

std::wstring ResolveAppExecutionAlias(const std::wstring& aliasPath) {
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

    // Validate the fixed header before touching any field.
    const size_t headerSize = offsetof(REPARSE_DATA_BUFFER, GenericReparseBuffer);
    if (bytesReturned < headerSize) {
        return L"";
    }

    PREPARSE_DATA_BUFFER reparseData = reinterpret_cast<PREPARSE_DATA_BUFFER>(buffer.data());
    if (reparseData->ReparseTag != IO_REPARSE_TAG_APPEXECLINK) {
        return L"";
    }

    // The AppExecLink payload is ReparseDataLength bytes starting at DataBuffer.
    // Clamp to what was actually returned to avoid reading past the buffer.
    size_t dataLen = reparseData->ReparseDataLength;
    size_t dataCapacity = (bytesReturned > headerSize) ? (bytesReturned - headerSize) : 0;
    if (dataLen > dataCapacity) {
        dataLen = dataCapacity; // truncated; still bounded
    }

    const BYTE* dataStart = reparseData->GenericReparseBuffer.DataBuffer;
    if (dataLen < 4) {
        return L""; // need at least the 4-byte version prefix
    }

    const BYTE* cursor = dataStart + 4; // skip 4-byte version
    size_t remaining = dataLen - 4;

    // Three consecutive NUL-terminated wide strings:
    //   1. Package Family Name
    //   2. Package Relative Application ID
    //   3. Target Path (the one we want)
    std::wstring str1, str2, str3;
    size_t consumed = 0;

    if (!ReadBoundedWideString(cursor, remaining, str1, consumed)) return L"";
    cursor += consumed; remaining -= consumed;

    if (!ReadBoundedWideString(cursor, remaining, str2, consumed)) return L"";
    cursor += consumed; remaining -= consumed;

    if (!ReadBoundedWideString(cursor, remaining, str3, consumed)) return L"";

    return str3;
}

std::wstring ResolveExecutableInCommand(const std::wstring& command) {
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
// Runs on a dedicated system thread. We do the minimum necessary here and avoid
// blocking on the cleanup mutex (which the main thread may hold); if the mutex
// is contended we still force termination rather than deadlock.
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    switch (ctrlType) {
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
    case CTRL_CLOSE_EVENT: {
        if (GlobalState::signalCleanupStarted.exchange(true)) {
            return TRUE;
        }

        GlobalState::isTerminating = true;

        // try_lock: if the main thread is mid-cleanup it holds the mutex; we
        // must not block here. If we can't take it, still terminate the job
        // (the most important containment action) and exit.
        std::unique_lock<std::mutex> lock(GlobalState::cleanupMutex, std::try_to_lock);

        if (GlobalState::activeJob) {
            TerminateJobObject(GlobalState::activeJob, 1);
            // Bounded drain; do not block the handler indefinitely.
            DrainJobObject(GlobalState::activeJob, 1000);
            if (lock.owns_lock()) GlobalState::activeJob = nullptr;
        }
        if (lock.owns_lock()) {
            if (!GlobalState::activeWorkspace.empty()) {
                SafeRemoveAll(GlobalState::activeWorkspace);
                GlobalState::activeWorkspace.clear();
            }
            if (!GlobalState::appContainerProfileName.empty()) {
                DeleteAppContainerProfile(GlobalState::appContainerProfileName.c_str());
                GlobalState::appContainerProfileName.clear();
            }
        }
        ExitProcess(1);
        return TRUE;
    }
    default:
        return FALSE;
    }
}
