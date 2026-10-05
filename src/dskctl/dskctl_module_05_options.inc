class DskctlOptions {
public:
    DskctlConfig config;
    std::vector<CleanTarget> targets = {
        {"User Temp Files", "--temp", "Cleans local user temp directory (%TEMP%)", {L"%TEMP%"}, false, false, "low"},
        {"System Temp Files", "--system-temp", "Cleans Windows system temp directory", {L"%WINDIR%\\Temp"}, false, false, "medium"},
        {"Windows Update Cache", "--update-cache", "Cleans Windows Update download packages", {L"%WINDIR%\\SoftwareDistribution\\Download"}, false, false, "medium"},
        {"Memory Crash Dumps", "--error-dumps", "Cleans MEMORY.DMP and Minidump files", {L"%WINDIR%\\MEMORY.DMP", L"%WINDIR%\\Minidump"}, false, true, "high"},
        {"System Logs", "--logs", "Cleans Windows Setup & System Log files", {L"%WINDIR%\\Logs", L"%WINDIR%\\Panther"}, false, true, "high"},
        {"Recycle Bin", "--recycle-bin", "Empties system-wide Recycle Bin", {}, false, true, "high"},
        {"Windows Error Reports", "--wer", "Cleans WER crash dumps and report queues", {L"%LOCALAPPDATA%\\CrashDumps", L"%PROGRAMDATA%\\Microsoft\\Windows\\WER"}, false, true, "high"},
        {"DirectX Shader Cache", "--shaders", "Cleans GPU/DirectX shader caches", {L"%LOCALAPPDATA%\\D3DSCache", L"%LOCALAPPDATA%\\NVIDIA\\DXCache", L"%LOCALAPPDATA%\\AMD\\DxCache"}, false, true, "medium"},
        {"Delivery Optimization", "--delivery-opt", "Cleans Delivery Optimization files", {L"%PROGRAMDATA%\\Microsoft\\Windows\\DeliveryOptimization\\Cache"}, false, true, "medium"},
        {"Internet Cache", "--inet-cache", "Cleans INetCache temporary files", {L"%LOCALAPPDATA%\\Microsoft\\Windows\\INetCache"}, false, true, "medium"}
    };

    bool showHelp = false;

    bool Parse(int argc, char* argv[]) {
        DskctlConfigManager::LoadConfig(DskctlConfigManager::GetConfigPath(), config);

        bool targetSpecified = false;
        bool safeOnly = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--json") { config.outputFormat = OutputFormat::Json; continue; }
            if (arg == "--csv") { config.outputFormat = OutputFormat::Csv; continue; }
            if (arg == "--table") { config.outputFormat = OutputFormat::Table; continue; }
            if (arg == "--pipe" && i + 1 < argc) { config.pipeCommand = argv[++i]; continue; }

            if (arg == "-h" || arg == "--help" || arg == "help") {
                showHelp = true;
                return true;
            } else if (arg == "-n" || arg == "--dry-run") {
                config.dryRun = true;
                config.execute = false;
            } else if (arg == "-x" || arg == "--execute") {
                config.execute = true;
                config.dryRun = false;
            } else if (arg == "-t" || arg == "--trash") {
                config.trash = true;
            } else if (arg == "-v" || arg == "--verbose") {
                config.verbose = true;
            } else if (arg == "-f" || arg == "--force") {
                config.force = true;
            } else if (arg == "-a" || arg == "--all") {
                for (auto& t : targets) t.selected = true;
                targetSpecified = true;
            } else if (arg == "-s" || arg == "--safe") {
                safeOnly = true;
                targetSpecified = true;
            } else {
                bool matched = false;
                for (auto& t : targets) {
                    if (arg == t.flag) {
                        t.selected = true;
                        targetSpecified = matched = true;
                        break;
                    }
                }
                if (!matched) {
                    std::cerr << RED << "[EINVAL] Invalid flag or target: " << arg << RESET << "\n";
                    std::cerr << "Run '" << argv[0] << " --help' for available flags.\n";
                    return false;
                }
            }
        }

        if (safeOnly) {
            for (auto& t : targets) {
                if (t.risk == "low" || t.risk == "medium") {
                    t.selected = true;
                }
            }
        } else if (!targetSpecified) {
            for (auto& t : targets) {
                if (t.risk == "low") {
                    t.selected = true;
                }
            }
        }

        return true;
    }
};
