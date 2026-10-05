// ============================================================================
// Command-Line & Argument Tokenizer (-S / --split-string support)
// ============================================================================

class ArgumentTokenizer {
public:
    // Implements POSIX/GNU env -S split-string parsing rules
    static std::vector<std::wstring> splitString(const std::wstring& str) {
        std::vector<std::wstring> tokens;
        std::wstring current;
        bool inSingleQuote = false;
        bool inDoubleQuote = false;
        bool escaping = false;

        for (size_t i = 0; i < str.length(); ++i) {
            wchar_t ch = str[i];

            if (escaping) {
                switch (ch) {
                    case L'n': current += L'\n'; break;
                    case L't': current += L'\t'; break;
                    case L'r': current += L'\r'; break;
                    case L'\\': current += L'\\'; break;
                    case L'\'': current += L'\''; break;
                    case L'\"': current += L'\"'; break;
                    case L'_': current += L' '; break; // GNU env \_ is space
                    case L'#': current += L'#'; break;
                    default: current += ch; break;
                }
                escaping = false;
                continue;
            }

            if (ch == L'\\' && !inSingleQuote) {
                escaping = true;
                continue;
            }

            if (ch == L'\'' && !inDoubleQuote) {
                inSingleQuote = !inSingleQuote;
                continue;
            }

            if (ch == L'\"' && !inSingleQuote) {
                inDoubleQuote = !inDoubleQuote;
                continue;
            }

            if (iswspace(ch) && !inSingleQuote && !inDoubleQuote) {
                if (!current.empty()) {
                    tokens.push_back(current);
                    current.clear();
                }
                continue;
            }

            current += ch;
        }

        if (!current.empty()) {
            tokens.push_back(current);
        }

        return tokens;
    }

    // Encodes a vector of arguments into a single Win32 CreateProcess command line
    static std::wstring buildWindowsCommandLine(const std::vector<std::wstring>& args) {
        std::wstring result;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) result += L' ';
            result += quoteWindowsArg(args[i]);
        }
        return result;
    }

private:
    static std::wstring quoteWindowsArg(const std::wstring& arg) {
        if (arg.empty()) return L"\"\"";

        bool needsQuotes = (arg.find_first_of(L" \t\n\v\"") != std::wstring::npos);
        if (!needsQuotes) return arg;

        std::wstring quoted = L"\"";
        int backslashes = 0;

        for (wchar_t ch : arg) {
            if (ch == L'\\') {
                backslashes++;
            } else if (ch == L'\"') {
                quoted.append(backslashes * 2 + 1, L'\\');
                quoted += L'\"';
                backslashes = 0;
            } else {
                quoted.append(backslashes, L'\\');
                backslashes = 0;
                quoted += ch;
            }
        }

        quoted.append(backslashes * 2, L'\\');
        quoted += L'\"';
        return quoted;
    }
};
