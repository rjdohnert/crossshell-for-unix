#include "engine.hpp"

std::wstring PwdEngine::stripExtendedPrefix(const std::wstring& path) {
    if (path.rfind(L"\\\\?\\UNC\\", 0) == 0) {
        return L"\\\\" + path.substr(8);
    } else if (path.rfind(L"\\\\?\\", 0) == 0) {
        return path.substr(4);
    }
    return path;
}

std::wstring PwdEngine::getLogicalCwd() {
    DWORD len = GetCurrentDirectoryW(0, nullptr);
    if (len == 0) return L"";
    std::wstring buffer(len, L'\0');
    DWORD written = GetCurrentDirectoryW(len, &buffer[0]);
    if (written > 0 && written < len) {
        buffer.resize(written);
        return buffer;
    }
    return L"";
}

std::wstring PwdEngine::getPhysicalCwd(const std::wstring& logicalPath) {
    HANDLE hDir = CreateFileW(
        logicalPath.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        nullptr
    );

    if (hDir == INVALID_HANDLE_VALUE) {
        return logicalPath;
    }

    DWORD len = GetFinalPathNameByHandleW(hDir, nullptr, 0, FILE_NAME_NORMALIZED);
    if (len == 0) {
        CloseHandle(hDir);
        return logicalPath;
    }

    std::wstring buffer(len, L'\0');
    DWORD written = GetFinalPathNameByHandleW(hDir, &buffer[0], len, FILE_NAME_NORMALIZED);
    CloseHandle(hDir);

    if (written > 0 && written < len) {
        buffer.resize(written);
        return stripExtendedPrefix(buffer);
    }

    return logicalPath;
}

int PwdEngine::execute(const PwdOptions& opts) {
    std::wstring logical = getLogicalCwd();
    if (logical.empty()) {
        std::wcerr << L"pwd: failed to get current directory\n";
        return 1;
    }

    if (opts.physical) {
        std::wstring physical = getPhysicalCwd(logical);
        std::wcout << physical << L"\n";
    } else {
        std::wcout << logical << L"\n";
    }
    return 0;
}
