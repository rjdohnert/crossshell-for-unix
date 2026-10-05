static std::string env_utf8(const std::wstring& value) {
    int size = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) return {};
    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), result.data(), size, nullptr, nullptr);
    return result;
}

// ============================================================================
// Case-Insensitive Comparator for Windows Environment Variables
// ============================================================================

struct CaseInsensitiveWStringCompare {
    bool operator()(const std::wstring& lhs, const std::wstring& rhs) const {
        return _wcsicmp(lhs.c_str(), rhs.c_str()) < 0;
    }
};

// ============================================================================
// Data Models & Configuration Options
// ============================================================================

struct ProgramOptions {
    bool ignoreEnvironment = false;        // -i, -, --ignore-environment
    bool nullDelimiter = false;            // -0, --null
    bool verbose = false;                  // -v, --verbose
    bool showHelp = false;                 // -?, --help
    bool showVersion = false;              // --version
    std::wstring changeDir;                // -C, --chdir=DIR
    std::vector<std::wstring> unsetVars;   // -u, --unset=NAME
    std::vector<std::pair<std::wstring, std::wstring>> setVars; // NAME=VALUE
    std::vector<std::wstring> commandAndArgs;                   // COMMAND [ARG]...
    int outputFormat = 0;
    std::wstring pipeCommand;
};
