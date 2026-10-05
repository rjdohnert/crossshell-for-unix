class CompiledPatternMatcher {
public:
    static bool IsCaseSensitive(const std::string& pattern, CaseMode mode) {
        if (mode == CaseMode::Sensitive) return true;
        if (mode == CaseMode::Insensitive) return false;
        return std::any_of(pattern.begin(), pattern.end(), [](unsigned char c) { return std::isupper(c); });
    }

    static CompiledPattern PreparePattern(const std::string& pattern, CaseMode mode, bool fixed) {
        CompiledPattern cp;
        cp.rawPattern = pattern;
        cp.isFixed = fixed;
        cp.caseSensitive = IsCaseSensitive(pattern, mode);

        if (!fixed && !pattern.empty()) {
            try {
                auto flags = std::regex_constants::ECMAScript;
                if (!cp.caseSensitive) flags |= std::regex_constants::icase;
                cp.regexObj = std::regex(pattern, flags);
            } catch (const std::regex_error&) {
                cp.isFixed = true;
            }
        }
        return cp;
    }

    static bool Matches(const std::string& filename, const CompiledPattern& cp) {
        if (cp.rawPattern.empty()) return true;

        if (cp.isFixed || !cp.regexObj.has_value()) {
            if (cp.caseSensitive) {
                return filename.find(cp.rawPattern) != std::string::npos;
            } else {
                return PathHelper::ToLower(filename).find(PathHelper::ToLower(cp.rawPattern)) != std::string::npos;
            }
        } else {
            return std::regex_search(filename, cp.regexObj.value());
        }
    }
};

class CommandExecutor {
public:
    static bool ExecuteCommandDirect(const std::string& commandLineUtf8) {
        std::wstring cmdLine = PathHelper::Utf8ToWide(commandLineUtf8);
        if (cmdLine.empty()) return false;

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};

        std::wstring mutableCmd = cmdLine;
        BOOL ok = CreateProcessW(
            nullptr,
            mutableCmd.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi
        );

        if (!ok) {
            return false;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }

    static std::string BuildExecCommand(const std::string& templateCmd, const fs::path& p) {
        std::string result = templateCmd;
        std::string fullPath = PathHelper::QuotePath(PathHelper::PathToUtf8(p));
        std::string fileName = PathHelper::QuotePath(PathHelper::PathToUtf8(p.filename()));
        std::string parentDir = PathHelper::QuotePath(PathHelper::PathToUtf8(p.parent_path()));
        std::string pathNoExt = PathHelper::QuotePath(PathHelper::PathToUtf8(p.parent_path() / p.stem()));

        auto replaceAll = [](std::string& str, const std::string& from, const std::string& to) {
            size_t start_pos = 0;
            while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length();
            }
        };

        replaceAll(result, "{//}", parentDir);
        replaceAll(result, "{/}", fileName);
        replaceAll(result, "{.}", pathNoExt);
        replaceAll(result, "{}", fullPath);

        return result;
    }
};
