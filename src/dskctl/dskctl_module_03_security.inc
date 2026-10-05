class SecurityInspector {
public:
    static bool IsCurrentUserAdmin() {
        BOOL isAdmin = FALSE;
        HANDLE hToken = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            ScopedProcessToken scopedToken(hToken);
            TOKEN_ELEVATION elevation;
            DWORD cbSize = sizeof(TOKEN_ELEVATION);
            if (GetTokenInformation(scopedToken.Get(), TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
                isAdmin = elevation.TokenIsElevated;
            }
        }
        return isAdmin != 0;
    }

    static std::vector<fs::path> BuildAllowedRoots() {
        std::vector<fs::path> roots;
        const std::vector<std::wstring> rawRoots = {
            L"%TEMP%",
            L"%WINDIR%\\Temp",
            L"%WINDIR%\\SoftwareDistribution\\Download",
            L"%WINDIR%\\Logs",
            L"%WINDIR%\\Panther",
            L"%WINDIR%\\MEMORY.DMP",
            L"%WINDIR%\\Minidump",
            L"%LOCALAPPDATA%\\CrashDumps",
            L"%PROGRAMDATA%\\Microsoft\\Windows\\WER",
            L"%LOCALAPPDATA%\\D3DSCache",
            L"%LOCALAPPDATA%\\NVIDIA\\DXCache",
            L"%LOCALAPPDATA%\\AMD\\DxCache",
            L"%PROGRAMDATA%\\Microsoft\\Windows\\DeliveryOptimization\\Cache",
            L"%LOCALAPPDATA%\\Microsoft\\Windows\\INetCache"
        };

        for (const auto& raw : rawRoots) {
            fs::path root = StringHelper::ExpandEnvPath(raw);
            if (!root.empty()) {
                roots.push_back(root.lexically_normal());
            }
        }
        return roots;
    }

    static bool IsAllowedPath(const fs::path& path) {
        if (path.empty()) {
            return false;
        }

        std::error_code ec;
        fs::path resolved = fs::weakly_canonical(path, ec);
        if (ec) {
            resolved = path;
        }

        std::wstring normalized = resolved.wstring();
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](wchar_t c) {
            return std::towlower(c);
        });

        auto isWithinRoot = [&](const fs::path& root) {
            std::wstring rootString = root.wstring();
            std::transform(rootString.begin(), rootString.end(), rootString.begin(), [](wchar_t c) {
                return std::towlower(c);
            });
            return normalized == rootString || (normalized.rfind(rootString, 0) == 0 && normalized.size() > rootString.size() && normalized[rootString.size()] == L'\\');
        };

        for (const auto& root : BuildAllowedRoots()) {
            if (isWithinRoot(root)) {
                return true;
            }
        }
        return false;
    }
};

class DskctlLogger {
public:
    static fs::path GetLogPath() {
        wchar_t buffer[MAX_PATH] = {};
        DWORD size = GetTempPathW(MAX_PATH, buffer);
        if (size == 0 || size >= MAX_PATH) {
            return fs::temp_directory_path() / "dskctl.log";
        }
        return fs::path(buffer) / "dskctl.log";
    }

    static void AppendLogLine(const std::string& line) {
        std::ofstream log(GetLogPath(), std::ios::app);
        if (log.is_open()) {
            log << line << "\n";
        }
    }
};

class DskctlConfigManager {
public:
    static fs::path GetConfigPath() {
        wchar_t buffer[MAX_PATH] = {};
        DWORD size = GetTempPathW(MAX_PATH, buffer);
        if (size == 0 || size >= MAX_PATH) {
            return fs::temp_directory_path() / "dskctl.conf";
        }
        return fs::path(buffer) / "dskctl.conf";
    }

    static bool LoadConfig(const fs::path& path, DskctlConfig& config) {
        std::ifstream input(path);
        if (!input.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(input, line)) {
            const size_t separator = line.find('=');
            if (separator == std::string::npos) {
                continue;
            }

            std::string key = StringHelper::ToLowerCopy(StringHelper::TrimCopy(line.substr(0, separator)));
            std::string value = StringHelper::TrimCopy(line.substr(separator + 1));
            if (key == "preview_only") {
                config.dryRun = (value == "1" || StringHelper::ToLowerCopy(value) == "true" || StringHelper::ToLowerCopy(value) == "yes");
                if (config.dryRun) {
                    config.execute = false;
                }
            } else if (key == "trash") {
                config.trash = (value == "1" || StringHelper::ToLowerCopy(value) == "true" || StringHelper::ToLowerCopy(value) == "yes");
            } else if (key == "force") {
                config.force = (value == "1" || StringHelper::ToLowerCopy(value) == "true" || StringHelper::ToLowerCopy(value) == "yes");
            } else if (key == "verbose") {
                config.verbose = (value == "1" || StringHelper::ToLowerCopy(value) == "true" || StringHelper::ToLowerCopy(value) == "yes");
            }
        }

        return true;
    }
};
