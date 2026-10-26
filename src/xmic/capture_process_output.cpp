#include "xmic_common.hpp"

bool RunProcessDirectCapture(const std::wstring& applicationPath, const std::wstring& commandLine, std::wstring& output, int& exitCode) {
    output.clear();
    exitCode = -1;

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

    STARTUPINFOW si = {};
    si.cb = sizeof(STARTUPINFOW);
    si.hStdError = hChildStdOutWrite;
    si.hStdOutput = hChildStdOutWrite;
    si.dwFlags |= STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi = {};
    std::vector<wchar_t> cmdBuffer(commandLine.begin(), commandLine.end());
    cmdBuffer.push_back(L'\0');

    BOOL created = CreateProcessW(
        applicationPath.empty() ? NULL : applicationPath.c_str(),
        cmdBuffer.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &si,
        &pi
    );

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
    while (ReadFile(hChildStdOutRead, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
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

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD processExitCode = 0;
    GetExitCodeProcess(pi.hProcess, &processExitCode);
    exitCode = static_cast<int>(processExitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hChildStdOutRead);

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
