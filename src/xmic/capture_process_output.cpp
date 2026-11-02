#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include "xmic_common.hpp"

bool RunProcessDirectCapture(const std::wstring& applicationPath, const std::wstring& commandLine, std::wstring& output, int& exitCode) {
    output.clear();
    exitCode = -1;

    constexpr ULONGLONG kProcessCaptureTimeoutMs = 5 * 60 * 1000;
    constexpr DWORD kTerminationWaitMs = 5000;

    SECURITY_ATTRIBUTES saAttr = {};
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hChildStdOutRead = NULL;
    HANDLE hChildStdOutWrite = NULL;
    if (!CreatePipe(&hChildStdOutRead, &hChildStdOutWrite, &saAttr, 0)) {
        return false;
    }

    if (!SetHandleInformation(hChildStdOutRead, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hChildStdOutRead);
        CloseHandle(hChildStdOutWrite);
        return false;
    }

    HANDLE childStdInput = NULL;
    HANDLE parentProcess = GetCurrentProcess();
    HANDLE currentStdInput = GetStdHandle(STD_INPUT_HANDLE);
    if (currentStdInput != NULL && currentStdInput != INVALID_HANDLE_VALUE) {
        if (!DuplicateHandle(parentProcess, currentStdInput, parentProcess, &childStdInput, 0, TRUE, DUPLICATE_SAME_ACCESS)) {
            CloseHandle(hChildStdOutRead);
            CloseHandle(hChildStdOutWrite);
            return false;
        }
    } else {
        childStdInput = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &saAttr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (childStdInput == INVALID_HANDLE_VALUE) {
            CloseHandle(hChildStdOutRead);
            CloseHandle(hChildStdOutWrite);
            return false;
        }
    }

    SIZE_T attributeListSize = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attributeListSize);
    if (attributeListSize == 0) {
        CloseHandle(childStdInput);
        CloseHandle(hChildStdOutRead);
        CloseHandle(hChildStdOutWrite);
        return false;
    }

    std::vector<unsigned char> attributeListStorage(attributeListSize);
    LPPROC_THREAD_ATTRIBUTE_LIST attributeList = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeListStorage.data());
    if (!InitializeProcThreadAttributeList(attributeList, 1, 0, &attributeListSize)) {
        CloseHandle(childStdInput);
        CloseHandle(hChildStdOutRead);
        CloseHandle(hChildStdOutWrite);
        return false;
    }

    HANDLE inheritedHandles[] = { childStdInput, hChildStdOutWrite };
    bool attributesConfigured = UpdateProcThreadAttribute(
        attributeList,
        0,
        PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
        inheritedHandles,
        sizeof(inheritedHandles),
        NULL,
        NULL
    ) != FALSE;

    STARTUPINFOEXW si = {};
    si.StartupInfo.cb = sizeof(STARTUPINFOEXW);
    si.StartupInfo.hStdInput = childStdInput;
    si.StartupInfo.hStdError = hChildStdOutWrite;
    si.StartupInfo.hStdOutput = hChildStdOutWrite;
    si.StartupInfo.dwFlags |= STARTF_USESTDHANDLES;
    si.lpAttributeList = attributeList;

    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
    cmdBuffer.push_back(L'\0');

    BOOL created = FALSE;
    if (attributesConfigured) {
        created = CreateProcessW(
            applicationPath.empty() ? NULL : applicationPath.c_str(),
            cmdBuffer.data(),
            NULL,
            NULL,
            TRUE,
            CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
            NULL,
            NULL,
            &si.StartupInfo,
            &pi
        );
    }

    DeleteProcThreadAttributeList(attributeList);
    CloseHandle(childStdInput);
    CloseHandle(hChildStdOutWrite);

    if (!created) {
        CloseHandle(hChildStdOutRead);
        return false;
    }

    std::string byteOutput;
    byteOutput.reserve(4096);

    char buffer[1024];
    DWORD bytesRead = 0;
    bool truncated = false;
    bool pipeClosed = false;
    bool processExited = false;
    bool captureFailed = false;
    ULONGLONG captureStartedAt = GetTickCount64();
    while (!pipeClosed || !processExited) {
        if (GetTickCount64() - captureStartedAt >= kProcessCaptureTimeoutMs) {
            captureFailed = true;
            break;
        }

        DWORD availableBytes = 0;
        if (!PeekNamedPipe(hChildStdOutRead, NULL, 0, NULL, &availableBytes, NULL)) {
            DWORD error = GetLastError();
            if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
                pipeClosed = true;
            } else {
                captureFailed = true;
                break;
            }
        } else if (availableBytes > 0) {
            DWORD requestedBytes = (std::min)(availableBytes, static_cast<DWORD>(sizeof(buffer)));
            if (!ReadFile(hChildStdOutRead, buffer, requestedBytes, &bytesRead, NULL)) {
                DWORD error = GetLastError();
                if (error == ERROR_BROKEN_PIPE || error == ERROR_PIPE_NOT_CONNECTED) {
                    pipeClosed = true;
                } else {
                    captureFailed = true;
                    break;
                }
            } else if (bytesRead > 0) {
                if (byteOutput.size() < kMaxCapturedOutputChars) {
                    size_t room = kMaxCapturedOutputChars - byteOutput.size();
                    size_t toAppend = (std::min)(static_cast<size_t>(bytesRead), room);
                    byteOutput.append(buffer, toAppend);
                    if (toAppend < bytesRead) {
                        truncated = true;
                    }
                } else {
                    truncated = true;
                }
            }
        }

        DWORD processWait = WaitForSingleObject(pi.hProcess, 0);
        if (processWait == WAIT_OBJECT_0) {
            processExited = true;
        } else if (processWait == WAIT_FAILED) {
            captureFailed = true;
            break;
        }

        if (!pipeClosed || !processExited) {
            Sleep(10);
        }
    }

    DWORD processExitCode = 0;
    if (captureFailed) {
        if (WaitForSingleObject(pi.hProcess, 0) == WAIT_TIMEOUT) {
            TerminateProcess(pi.hProcess, ERROR_TIMEOUT);
            WaitForSingleObject(pi.hProcess, kTerminationWaitMs);
        }
    } else if (!GetExitCodeProcess(pi.hProcess, &processExitCode)) {
        captureFailed = true;
        if (WaitForSingleObject(pi.hProcess, 0) == WAIT_TIMEOUT) {
            TerminateProcess(pi.hProcess, ERROR_GEN_FAILURE);
            WaitForSingleObject(pi.hProcess, kTerminationWaitMs);
        }
    } else {
        exitCode = static_cast<int>(processExitCode);
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStdOutRead);

    if (captureFailed) {
        return false;
    }

    if (truncated) {
        const std::string marker = "\n[...output truncated: limit reached...]\n";
        if (byteOutput.size() + marker.size() > kMaxCapturedOutputChars) {
            size_t keep = (kMaxCapturedOutputChars > marker.size())
                ? (kMaxCapturedOutputChars - marker.size())
                : 0;
            byteOutput.resize(keep);
        }
        byteOutput += marker;
    }

    if (byteOutput.empty()) {
        return true;
    }

    auto decodeBytes = [&](UINT codePage, int skipBytes = 0) -> bool {
        int inputLen = static_cast<int>(byteOutput.size()) - skipBytes;
        if (inputLen <= 0) {
            output.clear();
            return true;
        }

        const char* data = byteOutput.data() + skipBytes;
        int wlen = MultiByteToWideChar(codePage, 0, data, inputLen, NULL, 0);
        if (wlen <= 0) {
            return false;
        }

        output.resize(static_cast<size_t>(wlen));
        return MultiByteToWideChar(codePage, 0, data, inputLen, &output[0], wlen) > 0;
    };

    if (byteOutput.size() >= 2) {
        unsigned char b0 = static_cast<unsigned char>(byteOutput[0]);
        unsigned char b1 = static_cast<unsigned char>(byteOutput[1]);
        if (b0 == 0xFF && b1 == 0xFE) {
            size_t wcharCount = (byteOutput.size() - 2) / sizeof(wchar_t);
            const wchar_t* wideData = reinterpret_cast<const wchar_t*>(byteOutput.data() + 2);
            output.assign(wideData, wideData + wcharCount);
            return true;
        }
    }

    if (byteOutput.size() >= 3) {
        unsigned char b0 = static_cast<unsigned char>(byteOutput[0]);
        unsigned char b1 = static_cast<unsigned char>(byteOutput[1]);
        unsigned char b2 = static_cast<unsigned char>(byteOutput[2]);
        if (b0 == 0xEF && b1 == 0xBB && b2 == 0xBF) {
            if (decodeBytes(CP_UTF8, 3)) {
                return true;
            }
        }
    }

    if (decodeBytes(CP_UTF8)) {
        return true;
    }
    if (decodeBytes(CP_ACP)) {
        return true;
    }

    output.assign(byteOutput.begin(), byteOutput.end());
    return true;
}
bool RunPowerShellCommandCapture(const std::wstring& script, std::wstring& output, int& exitCode) {
    std::wstring powershellPath;
    if (!ResolveSystemExecutablePath(L"WindowsPowerShell\\v1.0\\powershell.exe", powershellPath)) {
        exitCode = -1;
        return false;
    }
    std::wstring encodedScript = EncodePowerShellCommandBase64(script);
    std::wstring commandLine = QuoteCommandLineArgument(powershellPath) + L" -NoProfile -ExecutionPolicy Bypass -EncodedCommand " + encodedScript;
    return RunProcessDirectCapture(powershellPath, commandLine, output, exitCode);
}
bool RunCommandCapture(const std::wstring& command, std::wstring& output, int& exitCode) {
    return RunProcessDirectCapture(L"", command, output, exitCode);
}
std::wstring EncodePowerShellCommandBase64(const std::wstring& script) {
    static const wchar_t* alphabet = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(script.data());
    size_t inputLen = script.size() * sizeof(wchar_t);

    std::wstring out;
    out.reserve(((inputLen + 2) / 3) * 4);

    size_t i = 0;
    while (i + 3 <= inputLen) {
        unsigned int b0 = bytes[i++];
        unsigned int b1 = bytes[i++];
        unsigned int b2 = bytes[i++];
        unsigned int n = (b0 << 16) | (b1 << 8) | b2;

        out += alphabet[(n >> 18) & 0x3F];
        out += alphabet[(n >> 12) & 0x3F];
        out += alphabet[(n >> 6) & 0x3F];
        out += alphabet[n & 0x3F];
    }

    size_t rem = inputLen - i;
    if (rem == 1) {
        unsigned int n = (bytes[i] << 16);
        out += alphabet[(n >> 18) & 0x3F];
        out += alphabet[(n >> 12) & 0x3F];
        out += L'=';
        out += L'=';
    } else if (rem == 2) {
        unsigned int n = (bytes[i] << 16) | (bytes[i + 1] << 8);
        out += alphabet[(n >> 18) & 0x3F];
        out += alphabet[(n >> 12) & 0x3F];
        out += alphabet[(n >> 6) & 0x3F];
        out += L'=';
    }

    return out;
}
