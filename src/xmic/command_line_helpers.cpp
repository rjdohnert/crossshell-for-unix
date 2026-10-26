#include "xmic_common.hpp"

bool ResolveExecutablePath(const std::wstring& candidate, std::wstring& resolvedPath) {
    std::vector<wchar_t> modulePath(32768);
    DWORD length = GetModuleFileNameW(nullptr, modulePath.data(), static_cast<DWORD>(modulePath.size()));
    if (length != 0 && length < modulePath.size()) {
        resolvedPath.assign(modulePath.data(), length);
        return true;
    }

    if (!candidate.empty()) {
        DWORD needed = GetFullPathNameW(candidate.c_str(), 0, nullptr, nullptr);
        if (needed == 0) {
            return false;
        }
        std::vector<wchar_t> fullPath(needed);
        DWORD written = GetFullPathNameW(candidate.c_str(), needed, fullPath.data(), nullptr);
        if (written == 0 || written >= needed) return false;
        resolvedPath.assign(fullPath.data(), written);
        return true;
    }

    return false;
}
bool ResolveSystemExecutablePath(const std::wstring& relativePath, std::wstring& resolvedPath) {
    resolvedPath.clear();
    std::vector<wchar_t> systemDirectory(32768);
    UINT length = GetSystemDirectoryW(systemDirectory.data(), static_cast<UINT>(systemDirectory.size()));
    if (length == 0 || length >= systemDirectory.size()) return false;

    resolvedPath.assign(systemDirectory.data(), length);
    if (!resolvedPath.empty() && resolvedPath.back() != L'\\') resolvedPath += L'\\';
    resolvedPath += relativePath;
    DWORD attributes = GetFileAttributesW(resolvedPath.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0;
}
bool ResolveExecutableOnPath(const std::wstring& executableName, std::wstring& resolvedPath) {
    resolvedPath.clear();
    DWORD required = GetEnvironmentVariableW(L"PATH", nullptr, 0);
    if (required == 0) return false;

    std::vector<wchar_t> pathBuffer(required);
    DWORD written = GetEnvironmentVariableW(L"PATH", pathBuffer.data(), required);
    if (written == 0 || written >= required) return false;

    std::wstring currentDirectory;
    DWORD cwdRequired = GetCurrentDirectoryW(0, nullptr);
    if (cwdRequired != 0) {
        std::vector<wchar_t> cwdBuffer(cwdRequired);
        DWORD cwdLength = GetCurrentDirectoryW(cwdRequired, cwdBuffer.data());
        if (cwdLength != 0 && cwdLength < cwdRequired) currentDirectory.assign(cwdBuffer.data(), cwdLength);
    }
    auto normalize = [](std::wstring path) {
        std::transform(path.begin(), path.end(), path.begin(), towlower);
        while (path.size() > 3 && (path.back() == L'\\' || path.back() == L'/')) path.pop_back();
        return path;
    };

    std::wstring pathList(pathBuffer.data(), written);
    size_t start = 0;
    while (start < pathList.size()) {
        size_t end = pathList.find(L';', start);
        if (end == std::wstring::npos) end = pathList.size();
        std::wstring directory = pathList.substr(start, end - start);
        start = end + 1;
        if (directory.empty()) continue;
        if (directory.size() >= 2 && directory.front() == L'"' && directory.back() == L'"') {
            directory = directory.substr(1, directory.size() - 2);
        }
        const bool driveAbsolute = directory.size() >= 3 && iswalpha(directory[0]) && directory[1] == L':' &&
            (directory[2] == L'\\' || directory[2] == L'/');
        const bool networkAbsolute = directory.size() >= 2 &&
            ((directory[0] == L'\\' && directory[1] == L'\\') || (directory[0] == L'/' && directory[1] == L'/'));
        if (!driveAbsolute && !networkAbsolute) continue;
        DWORD fullLength = GetFullPathNameW(directory.c_str(), 0, nullptr, nullptr);
        if (fullLength == 0) continue;
        std::vector<wchar_t> fullDirectory(fullLength);
        DWORD fullWritten = GetFullPathNameW(directory.c_str(), fullLength, fullDirectory.data(), nullptr);
        if (fullWritten == 0 || fullWritten >= fullLength) continue;
        directory.assign(fullDirectory.data(), fullWritten);
        if (!currentDirectory.empty() && normalize(directory) == normalize(currentDirectory)) continue;

        if (directory.back() != L'\\' && directory.back() != L'/') directory += L'\\';
        std::wstring candidate = directory + executableName;
        DWORD attributes = GetFileAttributesW(candidate.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            resolvedPath = std::move(candidate);
            return true;
        }
    }

    return false;
}
std::wstring QuoteCommandLineArgument(const std::wstring& value) {
    if (value.empty()) {
        return L"\"\"";
    }

    bool needsQuotes = value.find_first_of(L" \t\"") != std::wstring::npos;
    if (!needsQuotes) {
        return value;
    }

    std::wstring quoted = L"\"";
    for (size_t i = 0; i < value.size(); ++i) {
        size_t backslashes = 0;
        while (i < value.size() && value[i] == L'\\') {
            backslashes++;
            i++;
        }

        if (i == value.size()) {
            quoted.append(backslashes * 2, L'\\');
            break;
        } else if (value[i] == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(L'"');
        } else {
            quoted.append(backslashes, L'\\');
            quoted.push_back(value[i]);
        }
    }
    quoted += L"\"";
    return quoted;
}
std::wstring FormatSystemErrorMessage(DWORD errorCode) {
    LPWSTR messageBuffer = nullptr;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        0,
        reinterpret_cast<LPWSTR>(&messageBuffer),
        0,
        nullptr
    );

    std::wstring message = messageBuffer ? messageBuffer : L"Unknown system error";
    if (messageBuffer) {
        LocalFree(messageBuffer);
    }

    return message;
}
std::vector<std::wstring> TokenizeShellInput(const std::wstring& input) {
    std::vector<std::wstring> tokens;
    std::wstring current;
    bool inQuotes = false;

    for (wchar_t ch : input) {
        if (ch == L'\"') {
            inQuotes = !inQuotes;
            continue;
        }

        if (std::iswspace(ch) && !inQuotes) {
            if (!current.empty()) {
                tokens.push_back(current);
                current.clear();
            }
            continue;
        }

        current.push_back(ch);
    }

    if (!current.empty()) {
        tokens.push_back(current);
    }

    return tokens;
}
bool RunChildProcessAndWait(const std::wstring& executablePath, const std::vector<std::wstring>& args, std::wstring& errorOut, int& exitCode, bool suppressOutput) {
    std::wstring commandLine = QuoteCommandLineArgument(executablePath);
    for (const std::wstring& arg : args) {
        commandLine += L" " + QuoteCommandLineArgument(arg);
    }

    std::vector<wchar_t> commandBuffer(commandLine.begin(), commandLine.end());
    commandBuffer.push_back(L'\0');

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    PROCESS_INFORMATION processInfo{};
    HANDLE hNul = NULL;

    if (suppressOutput) {
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        hNul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hNul == INVALID_HANDLE_VALUE) {
            hNul = NULL;
        }
        if (hNul != NULL) {
            startupInfo.dwFlags |= STARTF_USESTDHANDLES;
            startupInfo.hStdOutput = hNul;
            startupInfo.hStdError = hNul;
            startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        }
    }

    std::wstring processPath = executablePath;
    if (processPath.empty()) {
        processPath = L"xmic.exe";
    }

    BOOL started = CreateProcessW(
        nullptr,
        commandBuffer.data(),
        nullptr,
        nullptr,
        TRUE,
        0,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo
    );

    if (!started) {
        DWORD errorCode = GetLastError();
        errorOut = L"Failed to launch xmic child process. Make sure xmic.exe is available and try again. System error: " + FormatSystemErrorMessage(errorCode);
        if (hNul != NULL) {
            CloseHandle(hNul);
        }
        return false;
    }

    WaitForSingleObject(processInfo.hProcess, INFINITE);

    DWORD code = 0;
    GetExitCodeProcess(processInfo.hProcess, &code);
    CloseHandle(processInfo.hThread);
    CloseHandle(processInfo.hProcess);
    if (hNul != NULL) {
        CloseHandle(hNul);
    }
    exitCode = static_cast<int>(code);
    return true;
}
bool TryParsePositiveInt(const std::wstring& input, int& valueOut) {
    valueOut = 0;
    if (input.empty()) {
        return false;
    }

    errno = 0;
    wchar_t* endPtr = nullptr;
    long parsed = wcstol(input.c_str(), &endPtr, 10);
    if (endPtr == input.c_str() || *endPtr != L'\0' || errno == ERANGE || parsed < kBenchmarkRunsMin || parsed > kBenchmarkRunsMax) {
        return false;
    }

    valueOut = static_cast<int>(parsed);
    return true;
}
bool TryParseBenchmarkRunsValue(int valueIndex, int argc, wchar_t* argv[], int& runsOut, std::wstring& errorOut) {
    runsOut = 0;
    errorOut.clear();

    if (valueIndex >= argc) {
        errorOut = kBenchmarkRunsMissingMessage;
        return false;
    }

    if (!TryParsePositiveInt(argv[valueIndex], runsOut)) {
        errorOut = kBenchmarkRunsInvalidMessage;
        return false;
    }

    return true;
}
bool ValidateBenchmarkOptions(int benchmarkRuns, bool benchmarkQuiet, bool interactivePasswordRequired, std::wstring& errorOut) {
    errorOut.clear();

    if (benchmarkRuns > 1 && interactivePasswordRequired) {
        errorOut = kBenchmarkInteractivePromptConflictMessage;
        return false;
    }

    if (benchmarkQuiet && benchmarkRuns <= 1) {
        errorOut = kBenchmarkQuietRequiresRunsMessage;
        return false;
    }

    return true;
}
