#include "system_info_provider.hpp"

std::wstring SystemInfoProvider::utf8ToWide(const std::string& str) {
        if (str.empty()) return std::wstring();
        int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), NULL, 0);
        std::wstring wstrTo(sizeNeeded, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), &wstrTo[0], sizeNeeded);
        return wstrTo;
    }

std::string SystemInfoProvider::getCurrentUserName() {
        wchar_t buffer[256];
        DWORD size = sizeof(buffer) / sizeof(wchar_t);
        if (GetUserNameW(buffer, &size)) {
            char mbBuf[256];
            WideCharToMultiByte(CP_UTF8, 0, buffer, -1, mbBuf, sizeof(mbBuf), NULL, NULL);
            return std::string(mbBuf);
        }
        return "UNKNOWN_USER";
    }

std::string SystemInfoProvider::getCurrentHostName() {
        wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD size = sizeof(buffer) / sizeof(wchar_t);
        if (GetComputerNameW(buffer, &size)) {
            char mbBuf[256];
            WideCharToMultiByte(CP_UTF8, 0, buffer, -1, mbBuf, sizeof(mbBuf), NULL, NULL);
            return std::string(mbBuf);
        }
        return "UNKNOWN_HOST";
    }

std::string SystemInfoProvider::getCurrentTimestamp() {
        auto now = std::chrono::system_clock::now();
        auto inTimeT = std::chrono::system_clock::to_time_t(now);
        std::tm tmStruct{};
        localtime_s(&tmStruct, &inTimeT);

        std::ostringstream oss;
        oss << std::put_time(&tmStruct, "%a %b %d %H:%M:%S %Y");
        return oss.str();
    }
