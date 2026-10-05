// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class JoinReporter {
public:
    static std::string toUtf8(const std::wstring& text) {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, NULL, NULL);
        return result;
    }

    static int dispatch(const std::wstring& content, int format, const std::wstring& pipeCommand) {
        std::wstring text;
        if (format == 1) {
            text = L"{\"joined\":\"" + content + L"\"}\n";
        } else if (format == 2) {
            text = L"joined\n\"" + content + L"\"\n";
        } else if (format == 3) {
            text = L"JOINED\n------\n" + content + L"\n";
        } else {
            text = content;
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _wpopen(pipeCommand.c_str(), L"w");
            if (!pipe) return 1;
            std::string utf8 = toUtf8(text);
            std::fwrite(utf8.data(), 1, utf8.size(), pipe);
            _pclose(pipe);
        } else {
            std::wcout << text;
        }
        return 0;
    }
};

