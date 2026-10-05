// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class GzipReporter {
public:
    static std::string toUtf8(const std::wstring& text) {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), &result[0], size, NULL, NULL);
        return result;
    }

    static int dispatch(const std::vector<std::pair<std::wstring, std::wstring>>& operations,
                        int format, const std::wstring& pipeCommand) {
        if (format == 0 && pipeCommand.empty()) return 0;

        std::wstring text;
        if (format == 1) {
            text = L"{\"operations\":[";
            for (size_t i = 0; i < operations.size(); ++i) {
                if (i > 0) text += L",";
                text += L"{\"source\":\"" + operations[i].first + L"\",\"target\":\"" + operations[i].second + L"\"}";
            }
            text += L"]}\n";
        } else if (format == 2) {
            text = L"source,target\n";
            for (const auto& op : operations) {
                text += L"\"" + op.first + L"\",\"" + op.second + L"\"\n";
            }
        } else if (format == 3) {
            text = L"SOURCE\tTARGET\n--------------------\n";
            for (const auto& op : operations) {
                text += op.first + L"\t" + op.second + L"\n";
            }
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

