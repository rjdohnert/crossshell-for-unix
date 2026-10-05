// ============================================================================
// 4. OUTPUT REPORTER
// ============================================================================

class HostReporter {
public:
    static void Emit(const HostOptions& opts, const std::vector<ResolvedRecord>& records) {
        if (opts.format == OutputFormat::Standard && opts.pipeCommand.empty()) {
            for (const auto& rec : records) {
                std::wcout << rec.formattedLine << L"\n";
            }
            return;
        }

        std::wstring text;
        if (opts.format == OutputFormat::Json) {
            text = L"[";
            for (size_t i = 0; i < records.size(); ++i) {
                if (i > 0) text += L",";
                text += L"{\"query\":\"" + opts.target + L"\",\"type\":\"" + records[i].type +
                        L"\",\"result\":\"" + records[i].value + L"\"}";
            }
            text += L"]\n";
        } else if (opts.format == OutputFormat::Csv) {
            text = L"query,type,result\n";
            for (const auto& rec : records) {
                text += opts.target + L"," + rec.type + L"," + rec.value + L"\n";
            }
        } else {
            text = L"QUERY\tTYPE\tRESULT\n";
            for (const auto& rec : records) {
                text += opts.target + L"\t" + rec.type + L"\t" + rec.value + L"\n";
            }
        }

        if (!opts.pipeCommand.empty()) {
            SendToPipe(opts.pipeCommand, text);
        } else {
            std::wcout << text;
        }
    }

private:
    static void SendToPipe(const std::wstring& pipeCmd, const std::wstring& text) {
        FILE* pipe = _wpopen(pipeCmd.c_str(), L"w");
        if (!pipe) {
            std::wcerr << L"host: failed to open pipe: " << pipeCmd << L"\n";
            return;
        }

        int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
        if (size > 0) {
            std::string narrow(static_cast<size_t>(size), '\0');
            WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), narrow.data(), size, nullptr, nullptr);
            fwrite(narrow.data(), 1, narrow.size(), pipe);
        }
        _pclose(pipe);
    }
};

