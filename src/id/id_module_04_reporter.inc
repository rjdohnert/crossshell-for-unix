// ============================================================================
// 4. OUTPUT REPORTER
// ============================================================================

class IdReporter {
public:
    static void Emit(const IdOptions& opts, const UserTokenData& data) {
        std::wostringstream output;
        if (opts.optUser) {
            output << (opts.optName ? data.user.accountName : data.user.sidString) << L"\n";
        } else if (opts.optGroup) {
            output << (opts.optName ? data.primaryGroup.accountName : data.primaryGroup.sidString) << L"\n";
        } else if (opts.optGroups) {
            for (size_t i = 0; i < data.groups.size(); ++i) {
                output << (opts.optName ? data.groups[i].accountName : data.groups[i].sidString);
                if (i + 1 < data.groups.size()) output << L" ";
            }
            output << L"\n";
        } else {
            output << L"uid=" << data.user.sidString << L"(" << data.user.accountName << L") ";
            output << L"gid=" << data.primaryGroup.sidString << L"(" << data.primaryGroup.accountName << L") ";
            output << L"groups=";
            for (size_t i = 0; i < data.groups.size(); ++i) {
                output << data.groups[i].sidString << L"(" << data.groups[i].accountName << L")";
                if (i + 1 < data.groups.size()) output << L",";
            }
            output << L"\n";
        }

        std::wstring raw = output.str();
        std::wstring formatted;
        if (opts.format == OutputFormat::Json) {
            formatted = L"{\"output\":\"" + EscapeJson(raw) + L"\"}\n";
        } else if (opts.format == OutputFormat::Csv) {
            formatted = L"output\n\"" + raw + L"\"\n";
        } else if (opts.format == OutputFormat::Table) {
            formatted = L"OUTPUT\n------\n" + raw;
        } else {
            formatted = raw;
        }

        if (!opts.pipeCommand.empty()) {
            SendToPipe(opts.pipeCommand, formatted);
        } else {
            std::wcout << formatted;
        }
    }

private:
    static std::wstring EscapeJson(const std::wstring& src) {
        std::wstring out;
        for (wchar_t ch : src) {
            if (ch == L'\\' || ch == L'"') {
                out.push_back(L'\\');
            } else if (ch == L'\n') {
                out += L"\\n";
                continue;
            } else if (ch == L'\r') {
                out += L"\\r";
                continue;
            }
            out.push_back(ch);
        }
        return out;
    }

    static void SendToPipe(const std::wstring& pipeCmd, const std::wstring& text) {
        FILE* pipe = _wpopen(pipeCmd.c_str(), L"w");
        if (!pipe) return;
        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (size > 0) {
            std::string narrow(static_cast<size_t>(size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), size, nullptr, nullptr);
            fwrite(narrow.data(), 1, narrow.size(), pipe);
        }
        _pclose(pipe);
    }
};

