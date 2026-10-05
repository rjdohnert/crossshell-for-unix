// ============================================================================
// 2. STRING & PATH HELPERS
// ============================================================================

class StringHelper {
public:
    static std::wstring ToWide(const std::string& input) {
        if (input.empty()) return L"";
        int needed = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, nullptr, 0);
        if (needed <= 0) {
            needed = MultiByteToWideChar(CP_ACP, 0, input.c_str(), -1, nullptr, 0);
            if (needed <= 0) return L"";
            std::vector<wchar_t> buffer(static_cast<size_t>(needed));
            if (MultiByteToWideChar(CP_ACP, 0, input.c_str(), -1, buffer.data(), needed) <= 0) return L"";
            return std::wstring(buffer.data());
        }
        std::vector<wchar_t> buffer(static_cast<size_t>(needed));
        if (MultiByteToWideChar(CP_UTF8, 0, input.c_str(), -1, buffer.data(), needed) <= 0) return L"";
        return std::wstring(buffer.data());
    }

    static std::string ToNarrow(const std::wstring& input) {
        if (input.empty()) return "";
        int needed = WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (needed <= 0) return "";
        std::vector<char> buffer(static_cast<size_t>(needed));
        if (WideCharToMultiByte(CP_UTF8, 0, input.c_str(), -1, buffer.data(), needed, nullptr, nullptr) <= 0) return "";
        return std::string(buffer.data());
    }

    static std::string GetWeekdayName(WORD dayOfWeek) {
        static const char* kWeekdays[] = {
            "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
        };
        if (dayOfWeek > 6) return "Unknown";
        return kWeekdays[dayOfWeek];
    }

    static std::wstring FormatBytes(unsigned long long bytes) {
        static const wchar_t* suffixes[] = { L"B", L"KB", L"MB", L"GB", L"TB" };
        int suffixIndex = 0;
        double doubleBytes = static_cast<double>(bytes);
        while (doubleBytes >= 1024.0 && suffixIndex < 4) {
            doubleBytes /= 1024.0;
            suffixIndex++;
        }
        std::wstringstream ss;
        ss << std::fixed << std::setprecision(suffixIndex == 0 ? 0 : 2) << doubleBytes << L" " << suffixes[suffixIndex];
        return ss.str();
    }

    static std::vector<std::wstring> SplitArguments(const std::wstring& input) {
        std::vector<std::wstring> args;
        std::wstring current;
        bool in_quotes = false;
        for (size_t i = 0; i < input.length(); ++i) {
            wchar_t c = input[i];
            if (c == L'"') {
                in_quotes = !in_quotes;
            } else if (c == L' ' && !in_quotes) {
                if (!current.empty()) {
                    args.push_back(current);
                    current.clear();
                }
            } else {
                current += c;
            }
        }
        if (!current.empty()) {
            args.push_back(current);
        }
        return args;
    }
};

class PathHelper {
public:
    static std::wstring GetCurrentDirectoryString() {
        DWORD length = GetCurrentDirectoryW(0, NULL);
        if (length == 0) return L".";
        std::wstring path(length, L'\0');
        DWORD written = GetCurrentDirectoryW(length, &path[0]);
        if (written == 0) return L".";
        path.resize(written);
        return path;
    }

    static std::wstring JoinPath(const std::wstring& base, const std::wstring& child) {
        if (base.empty()) return child;
        if (base.back() == L'\\' || base.back() == L'/') return base + child;
        return base + L"\\" + child;
    }

    static std::wstring NormalizeSeparators(const std::wstring& input) {
        std::wstring normalized = input;
        for (wchar_t& ch : normalized) {
            if (ch == L'/') ch = L'\\';
        }
        return normalized;
    }

    static bool PathExists(const std::wstring& path) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES;
    }

    static bool IsDirectoryPath(const std::wstring& path) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }

    static std::wstring NormalizePath(const std::wstring& base, const std::wstring& input) {
        std::wstring normalizedInput = NormalizeSeparators(input);
        bool isDriveQualified = normalizedInput.length() >= 2 && normalizedInput[1] == L':';
        bool isUncPath = normalizedInput.length() >= 2 && normalizedInput[0] == L'\\' && normalizedInput[1] == L'\\';
        bool isRootedPath = normalizedInput.length() >= 1 && (normalizedInput[0] == L'\\' || normalizedInput[0] == L'/');

        std::wstring combined = normalizedInput;
        if (!isDriveQualified && !isUncPath && !isRootedPath) {
            combined = JoinPath(base, normalizedInput);
        }

        DWORD result = GetFullPathNameW(combined.c_str(), 0, nullptr, nullptr);
        if (result == 0) return combined;

        std::wstring fullPath(result, L'\0');
        DWORD written = GetFullPathNameW(combined.c_str(), result, &fullPath[0], nullptr);
        if (written == 0 || written >= result) return combined;

        fullPath.resize(written);
        return fullPath;
    }

    static bool EnsureParentDirectoryExists(const std::wstring& path) {
        std::wstring parent = path;
        size_t slash = parent.find_last_of(L"\\/");
        if (slash == std::wstring::npos) return true;

        parent = parent.substr(0, slash);
        if (parent.empty()) return true;

        if (PathExists(parent)) return IsDirectoryPath(parent);

        std::wstring current = parent;
        std::vector<std::wstring> parts;
        while (!PathExists(current) && !current.empty()) {
            parts.push_back(current);
            size_t split = current.find_last_of(L"\\/");
            if (split == std::wstring::npos || split == 0) break;
            current = current.substr(0, split);
        }

        while (!parts.empty()) {
            std::wstring toCreate = parts.back();
            parts.pop_back();
            if (!CreateDirectoryW(toCreate.c_str(), nullptr)) {
                DWORD err = GetLastError();
                if (err != ERROR_ALREADY_EXISTS) return false;
            }
        }
        return true;
    }

    static std::wstring FileTimeToString(const FILETIME& ft) {
        SYSTEMTIME stUTC, stLocal;
        FileTimeToSystemTime(&ft, &stUTC);
        SystemTimeToTzSpecificLocalTime(NULL, &stUTC, &stLocal);

        std::wstringstream ss;
        ss << std::setfill(L'0')
           << std::setw(2) << stLocal.wMonth << L"/"
           << std::setw(2) << stLocal.wDay << L"/"
           << std::setw(4) << stLocal.wYear << L" "
           << std::setw(2) << stLocal.wHour << L":"
           << std::setw(2) << stLocal.wMinute << L":"
           << std::setw(2) << stLocal.wSecond;
        return ss.str();
    }
};

