class ConsoleEnvironment {
public:
    static bool InitConsole() {
        SetConsoleOutputCP(CP_UTF8);
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return false;

        DWORD dwMode = 0;
        if (!GetConsoleMode(hOut, &dwMode)) return false;
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        return SetConsoleMode(hOut, dwMode) != 0;
    }

    static bool ShouldUseColorByDefault() {
        if (!_isatty(_fileno(stdout))) return false;

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return false;

        DWORD mode = 0;
        return GetConsoleMode(hOut, &mode) != 0;
    }
};

class PathHelper {
public:
    static std::string ToLower(const std::string& input) {
        std::string result = input;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    static std::wstring Utf8ToWide(const std::string& s) {
        if (s.empty()) return std::wstring();
        int count = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (count <= 0) return std::wstring();
        std::wstring out(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), count);
        if (!out.empty() && out.back() == L'\0') {
            out.pop_back();
        }
        return out;
    }

    static std::string PathToUtf8(const fs::path& p) {
        auto u8 = p.u8string();
        return std::string(u8.begin(), u8.end());
    }

    static bool IsHidden(const fs::directory_entry& entry) {
        std::wstring wpath = entry.path().wstring();
        DWORD attrs = GetFileAttributesW(wpath.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_HIDDEN)) {
            return true;
        }
        std::wstring filename = entry.path().filename().wstring();
        return (!filename.empty() && filename[0] == L'.');
    }

    static bool IsExecutable(const fs::directory_entry& entry) {
        if (!entry.is_regular_file()) return false;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return (ext == ".exe" || ext == ".cmd" || ext == ".bat" || ext == ".com" || ext == ".ps1");
    }

    static bool IsIgnoredDirectory(const std::string& dirName, bool includeIgnored) {
        if (includeIgnored) return false;
        return DEFAULT_IGNORED_DIRS.find(ToLower(dirName)) != DEFAULT_IGNORED_DIRS.end();
    }

    static std::string QuoteForCreateProcessArg(const std::string& arg) {
        std::string out;
        out.push_back('"');

        size_t backslashes = 0;
        for (char c : arg) {
            if (c == '\\') {
                backslashes++;
                continue;
            }

            if (c == '"') {
                out.append(backslashes * 2 + 1, '\\');
                out.push_back('"');
                backslashes = 0;
                continue;
            }

            if (backslashes > 0) {
                out.append(backslashes, '\\');
                backslashes = 0;
            }
            out.push_back(c);
        }

        if (backslashes > 0) {
            out.append(backslashes * 2, '\\');
        }

        out.push_back('"');
        return out;
    }

    static std::string QuotePath(const std::string& pathStr) {
        return QuoteForCreateProcessArg(pathStr);
    }

    static std::string FormatPath(const fs::directory_entry& entry, const std::string& pathStr, bool useColor) {
        if (!useColor) return pathStr;

        if (entry.is_directory()) {
            return Color::CYAN + pathStr + Color::RESET;
        } else if (entry.is_symlink()) {
            return Color::MAGENTA + pathStr + Color::RESET;
        } else if (IsExecutable(entry)) {
            return Color::GREEN + pathStr + Color::RESET;
        }
        return pathStr;
    }
};
