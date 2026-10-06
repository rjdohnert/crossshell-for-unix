#include "string_utils.hpp"

std::string StringUtils::Utf8(const std::wstring& value) {
        int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

std::wstring StringUtils::ToLowerCopy(const std::wstring& value) {
        std::wstring lower;
        lower.reserve(value.size());
        for (wchar_t ch : value) {
            lower.push_back(static_cast<wchar_t>(towlower(ch)));
        }
        return lower;
    }

bool StringUtils::ParsePid(const std::wstring& text, DWORD& pid) {
        try {
            size_t consumed = 0;
            unsigned long parsed = std::stoul(text, &consumed, 10);
            if (consumed != text.size() || parsed == 0 || parsed > 0xFFFFFFFFUL) {
                return false;
            }
            pid = static_cast<DWORD>(parsed);
            return true;
        } catch (...) {
            return false;
        }
    }
