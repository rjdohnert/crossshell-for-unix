// ============================================================================
// 2. STRING & FORMATTING HELPERS
// ============================================================================

class StringHelper {
public:
    static std::wstring ToLowerCopy(std::wstring value) {
        std::transform(value.begin(), value.end(), value.begin(), [](wchar_t ch) {
            return static_cast<wchar_t>(towlower(ch));
        });
        return value;
    }

    static bool ContainsICase(const std::wstring& haystack, const std::wstring& needle) {
        if (needle.empty()) {
            return true;
        }
        return ToLowerCopy(haystack).find(ToLowerCopy(needle)) != std::wstring::npos;
    }

    static std::string ToUtf8(const std::wstring& text) {
        if (text.empty()) return {};
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        std::string result(size, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
        return result;
    }

    static std::wstring EscapeJson(const std::wstring& text) {
        std::wstring result;
        for (wchar_t ch : text) {
            if (ch == L'\\') result += L"\\\\";
            else if (ch == L'\"') result += L"\\\"";
            else result += ch;
        }
        return result;
    }
};

