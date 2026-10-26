#include "sysctl.hpp"

namespace sysctl {

void ThrowWindowsError(DWORD error, const std::string& operation) {
    throw std::system_error(static_cast<int>(error), std::system_category(), operation);
}

std::string Trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) return {};
    if (text.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
        throw std::length_error("Text is too long to convert to UTF-8");
    }
    const int length = static_cast<int>(text.size());
    const int size = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), length,
                                         nullptr, 0, nullptr, nullptr);
    if (size == 0) ThrowWindowsError(GetLastError(), "WideCharToMultiByte");
    std::string result(static_cast<size_t>(size), '\0');
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(), length,
                            result.data(), size, nullptr, nullptr) == 0) {
        ThrowWindowsError(GetLastError(), "WideCharToMultiByte");
    }
    return result;
}

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) return {};
    if (text.size() > static_cast<size_t>((std::numeric_limits<int>::max)())) {
        throw std::length_error("Text is too long to convert from UTF-8");
    }
    const int length = static_cast<int>(text.size());
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length, nullptr, 0);
    if (size == 0) ThrowWindowsError(GetLastError(), "MultiByteToWideChar");
    std::wstring result(static_cast<size_t>(size), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), length,
                           result.data(), size) == 0) {
        ThrowWindowsError(GetLastError(), "MultiByteToWideChar");
    }
    return result;
}

} // namespace sysctl
