// ============================================================================
// 3. FIELD SPLITTER & JOIN ENGINE
// ============================================================================

class FieldExtractor {
public:
    static std::vector<std::wstring> splitFields(const std::wstring& line, wchar_t delim) {
        std::vector<std::wstring> out;
        if (delim == L'\0') {
            std::wstringstream ss(line);
            std::wstring tok;
            while (ss >> tok) out.push_back(tok);
            return out;
        }

        std::wstring cur;
        for (wchar_t ch : line) {
            if (ch == delim) {
                out.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(ch);
            }
        }
        out.push_back(cur);
        return out;
    }

    static std::wstring joinLine(const std::vector<std::wstring>& f1, int k1,
                                const std::vector<std::wstring>& f2, int k2, wchar_t delim) {
        wchar_t d = (delim == L'\0') ? L' ' : delim;
        std::wstring out;
        auto append = [&](const std::wstring& s) {
            if (!out.empty()) out.push_back(d);
            out += s;
        };

        append((k1 >= 0 && k1 < static_cast<int>(f1.size())) ? f1[k1] : L"");
        for (int i = 0; i < static_cast<int>(f1.size()); ++i) {
            if (i != k1) append(f1[i]);
        }
        for (int i = 0; i < static_cast<int>(f2.size()); ++i) {
            if (i != k2) append(f2[i]);
        }
        return out;
    }
};

class JoinEngine {
private:
    JoinOptions options;

    static std::vector<std::wstring> readLines(const std::wstring& path) {
        std::vector<std::wstring> out;
        if (path == L"-") {
            for (std::wstring l; std::getline(std::wcin, l); ) out.push_back(l);
        } else {
            std::wifstream in(path);
            for (std::wstring l; std::getline(in, l); ) out.push_back(l);
        }
        return out;
    }

public:
    explicit JoinEngine(JoinOptions opts) : options(std::move(opts)) {}

    int execute() {
        std::vector<std::wstring> l1 = readLines(options.file1);
        std::vector<std::wstring> l2 = readLines(options.file2);

        int k1 = options.field1 - 1;
        int k2 = options.field2 - 1;

        std::vector<std::vector<std::wstring>> f1, f2;
        for (const auto& s : l1) f1.push_back(FieldExtractor::splitFields(s, options.delim));
        for (const auto& s : l2) f2.push_back(FieldExtractor::splitFields(s, options.delim));

        std::vector<bool> used1(f1.size(), false);
        std::vector<bool> used2(f2.size(), false);

        std::wostringstream captured;
        std::wostream* outStream = (options.outputFormat || !options.pipeCommand.empty()) ? &captured : &std::wcout;

        bool suppressJoined = (options.unpairedOnly1 || options.unpairedOnly2);

        for (size_t i = 0; i < f1.size(); ++i) {
            std::wstring key1 = (k1 >= 0 && k1 < static_cast<int>(f1[i].size())) ? f1[i][k1] : L"";
            bool matched = false;

            for (size_t j = 0; j < f2.size(); ++j) {
                std::wstring key2 = (k2 >= 0 && k2 < static_cast<int>(f2[j].size())) ? f2[j][k2] : L"";
                if (!key1.empty() && key1 == key2) {
                    matched = true;
                    used1[i] = true;
                    used2[j] = true;
                    if (!suppressJoined) {
                        *outStream << FieldExtractor::joinLine(f1[i], k1, f2[j], k2, options.delim) << L"\n";
                    }
                }
            }
        }

        if (options.includeUnpaired1 || options.unpairedOnly1) {
            for (size_t i = 0; i < f1.size(); ++i) {
                if (!used1[i]) *outStream << l1[i] << L"\n";
            }
        }

        if (options.includeUnpaired2 || options.unpairedOnly2) {
            for (size_t j = 0; j < f2.size(); ++j) {
                if (!used2[j]) *outStream << l2[j] << L"\n";
            }
        }

        if (options.outputFormat || !options.pipeCommand.empty()) {
            JoinReporter::dispatch(captured.str(), options.outputFormat, options.pipeCommand);
        }

        return 0;
    }
};

