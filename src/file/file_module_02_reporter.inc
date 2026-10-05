class FileReporter {
public:
    static std::string toUtf8(const std::wstring& text) {
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), NULL, 0, NULL, NULL);
        if (size <= 0) return {};
        std::string result(static_cast<size_t>(size), '\0');
        WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, NULL, NULL);
        return result;
    }

    static int dispatch(const std::vector<std::pair<std::wstring, std::wstring>>& results,
                        int format, bool brief, const std::wstring& pipeCommand) {
        std::wstring text;
        if (format == 1) {
            text = L"{\"files\":[";
            for (size_t i = 0; i < results.size(); ++i) {
                if (i > 0) text += L",";
                text += L"{\"path\":\"" + results[i].first + L"\",\"type\":\"" + results[i].second + L"\"}";
            }
            text += L"]}\n";
        } else if (format == 2) {
            text = L"path,type\n";
            for (const auto& r : results) {
                text += L"\"" + r.first + L"\",\"" + r.second + L"\"\n";
            }
        } else if (format == 3) {
            text = L"PATH\tTYPE\n--------------------\n";
            for (const auto& r : results) {
                text += r.first + L"\t" + r.second + L"\n";
            }
        } else {
            for (const auto& r : results) {
                if (brief) text += r.second + L"\n";
                else text += r.first + L": " + r.second + L"\n";
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
