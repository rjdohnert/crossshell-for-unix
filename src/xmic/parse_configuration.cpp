#include "xmic_common.hpp"

bool TryParseSshStrictHostKeyCheckingMode(const std::wstring& input, std::wstring& normalizedMode) {
    std::wstring value = input;
    std::transform(value.begin(), value.end(), value.begin(), towlower);

    if (value == L"yes" || value == L"no" || value == L"ask" || value == L"accept-new") {
        normalizedMode = value;
        return true;
    }

    return false;
}
bool TryParseTransportMode(const std::wstring& input, TransportMode& mode) {
    std::wstring value = input;
    std::transform(value.begin(), value.end(), value.begin(), towlower);

    if (value == L"dcom" || value == L"rpc" || value == L"dcom-rpc") {
        mode = TransportMode::DcomRpc;
        return true;
    }

    if (value == L"winrm" || value == L"wsman" || value == L"ws-management") {
        mode = TransportMode::WinRm;
        return true;
    }

    if (value == L"ssh") {
        mode = TransportMode::Ssh;
        return true;
    }

    return false;
}
bool TryParseWinRmBackendMode(const std::wstring& input, WinRmBackendMode& mode) {
    std::wstring value = input;
    std::transform(value.begin(), value.end(), value.begin(), towlower);

    if (value == L"powershell" || value == L"ps") {
        mode = WinRmBackendMode::PowerShell;
        return true;
    }

    if (value == L"native" || value == L"wwsapi" || value == L"cpp") {
        mode = WinRmBackendMode::Native;
        return true;
    }

    return false;
}
bool TryParseConsoleEncoding(const std::wstring& input, ConsoleEncoding& encoding) {
    std::wstring value = input;
    std::transform(value.begin(), value.end(), value.begin(), towlower);

    if (value == L"utf16" || value == L"utf-16") {
        encoding = ConsoleEncoding::Utf16;
        return true;
    }

    if (value == L"utf8" || value == L"utf-8") {
        encoding = ConsoleEncoding::Utf8;
        return true;
    }

    return false;
}
bool ConfigureConsoleOutputMode(ConsoleEncoding encoding) {
    if (encoding == ConsoleEncoding::Utf8) {
        if (!SetConsoleOutputCP(CP_UTF8)) {
            return false;
        }
        SetConsoleCP(CP_UTF8);
        if (_setmode(_fileno(stdout), _O_U8TEXT) == -1) {
            return false;
        }
        if (_setmode(_fileno(stderr), _O_U8TEXT) == -1) {
            return false;
        }
        return true;
    }

    if (_setmode(_fileno(stdout), _O_U16TEXT) == -1) {
        return false;
    }
    if (_setmode(_fileno(stderr), _O_U16TEXT) == -1) {
        return false;
    }
    return true;
}
bool ReadHiddenConsoleLine(const std::wstring& prompt, std::wstring& value, std::wstring& error) {
    value.clear();
    error.clear();

    HANDLE hInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD originalMode = 0;

    // Fallback for redirected stdin (pipe/file): read plain text input.
    if (hInput == NULL || hInput == INVALID_HANDLE_VALUE || !GetConsoleMode(hInput, &originalMode)) {
        if (!prompt.empty()) {
            std::wcout << prompt;
        }
        if (std::getline(std::wcin, value)) {
            return true;
        }
        error = L"Failed to read input from standard input stream.";
        return false;
    }

    HANDLE hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOutput != NULL && hOutput != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteConsoleW(hOutput, prompt.c_str(), static_cast<DWORD>(prompt.size()), &written, NULL);
    } else {
        std::wcout << prompt;
    }

    DWORD hiddenMode = originalMode & ~ENABLE_ECHO_INPUT;
    if (!SetConsoleMode(hInput, hiddenMode)) {
        error = L"Failed to disable console input echo for password entry.";
        return false;
    }

    wchar_t buffer[512] = {};
    DWORD charsRead = 0;
    BOOL readOk = ReadConsoleW(hInput, buffer, 511, &charsRead, NULL);

    SetConsoleMode(hInput, originalMode);

    if (hOutput != NULL && hOutput != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteConsoleW(hOutput, L"\n", 1, &written, NULL);
    } else {
        std::wcout << L"\n";
    }

    if (!readOk) {
        SecureZeroMemory(buffer, sizeof(buffer));
        error = L"Failed to read password from console.";
        return false;
    }

    while (charsRead > 0 && (buffer[charsRead - 1] == L'\n' || buffer[charsRead - 1] == L'\r')) {
        --charsRead;
    }

    value.assign(buffer, charsRead);
    SecureZeroMemory(buffer, sizeof(buffer));
    return true;
}
double ElapsedMilliseconds(const std::chrono::steady_clock::time_point& start, const std::chrono::steady_clock::time_point& end) {
    return std::chrono::duration<double, std::milli>(end - start).count();
}
