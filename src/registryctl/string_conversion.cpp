#include "string_conversion.hpp"

std::wstring Utils::ToWString(const std::string& str) {
        if (str.empty()) return L"";
        int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), nullptr, 0);
        std::wstring wstr(size, 0);
        MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], size);
        return wstr;
    }

std::string Utils::ToString(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
        std::string str(size, 0);
        WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str[0], size, nullptr, nullptr);
        return str;
    }

std::string Utils::ToUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(::toupper(c)); });
        return s;
    }

std::string Utils::FormatWin32Error(DWORD errorCode) {
        LPWSTR buf = nullptr;
        DWORD size = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, errorCode, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
        std::string msg = buf ? ToString(std::wstring(buf, size)) : "Unknown system error";
        if (buf) LocalFree(buf);
        while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r')) msg.pop_back();
        return "[" + std::to_string(errorCode) + "] " + msg;
    }

std::string Utils::BytesToHex(const std::vector<uint8_t>& bytes) {
        std::ostringstream ss;
        for (uint8_t b : bytes) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(b);
        }
        return ss.str();
    }

std::vector<uint8_t> Utils::HexToBytes(const std::string& hex) {
        std::vector<uint8_t> bytes;
        std::string clean;
        for (char c : hex) {
            if (::isxdigit(static_cast<unsigned char>(c))) clean.push_back(c);
        }
        for (size_t i = 0; i + 1 < clean.length(); i += 2) {
            std::string byteStr = clean.substr(i, 2);
            bytes.push_back(static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16)));
        }
        return bytes;
    }
