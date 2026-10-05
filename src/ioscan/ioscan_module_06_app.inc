// ============================================================================
// 6. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

class OptionParser {
public:
    static bool TakeValue(const std::wstring& arg, size_t& position, int argc, wchar_t* argv[], int& index,
                          const wchar_t option, std::wstring& value) {
        if (position + 1 < arg.size()) {
            value = arg.substr(position + 1);
            position = arg.size();
            return true;
        }
        if (index + 1 < argc) {
            value = argv[++index];
            position = arg.size();
            return true;
        }
        std::wcerr << L"ioscan: option -" << option << L" requires an argument\n";
        return false;
    }

    bool Parse(int argc, wchar_t* argv[], IoscanOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                OutputFormatter::PrintHelp(argv[0]);
                exitEarly = true;
                return true;
            }

            if (arg == L"--json") { opts.outputFormat = 1; continue; }
            if (arg == L"--csv") { opts.outputFormat = 2; continue; }
            if (arg == L"--table") { opts.outputFormat = 3; continue; }
            if (arg == L"--summary") { opts.summaryView = true; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }
            if (arg == L"--pipe") {
                std::wcerr << L"ioscan: option --pipe requires an argument\n";
                return false;
            }

            if (arg.length() > 1 && (arg[0] == L'-' || arg[0] == L'/')) {
                for (size_t j = 1; j < arg.length(); ++j) {
                    wchar_t c = arg[j];
                    if (c == L'f') opts.fullView = true;
                    else if (c == L'F') opts.compactView = true;
                    else if (c == L'N') opts.agileView = true;
                    else if (c == L'n') opts.listDeviceFiles = true;
                    else if (c == L'A') opts.showAlias = true;
                    else if (c == L'a') opts.processorThreads = true;
                    else if (c == L'B') opts.listPendingBindings = true;
                    else if (c == L'e') opts.showDevicePath = true;
                    else if (c == L'k') { opts.kernelOnly = true; opts.presentOnly = false; }
                    else if (c == L'u') { opts.usableOnly = true; }
                    else if (c == L'l') opts.localOnly = true;
                    else if (c == L's') { opts.staleOnly = true; opts.presentOnly = false; }
                    else if (c == L't') opts.showScanTime = true;
                    else if (c == L'U') opts.unclaimedInterfacesOnly = true;
                    else if (c == L'C' || c == L'd' || c == L'H' || c == L'I' || c == L'm' || c == L'P') {
                        std::wstring value;
                        if (!TakeValue(arg, j, argc, argv, i, c, value)) return false;
                        if (c == L'C') opts.classFilter = DeviceMapper::MapHpUxClassToWindows(value);
                        else if (c == L'd') opts.driverFilter = value;
                        else if (c == L'H') opts.hwPathFilter = value;
                        else if (c == L'm') opts.mappingKeyword = value;
                        else if (c == L'P') opts.propertyName = value;
                        else {
                            try {
                                size_t consumed = 0;
                                opts.instanceFilter = std::stoi(value, &consumed);
                                if (consumed != value.size() || opts.instanceFilter < 0) throw std::invalid_argument("instance");
                            } catch (...) {
                                std::wcerr << L"ioscan: invalid instance number: " << value << L"\n";
                                return false;
                            }
                        }
                        break;
                    } else if (c == L'D' || c == L'M' || c == L'R') {
                        std::wcerr << L"ioscan: -" << c
                                   << L" changes HP-UX kernel driver bindings and is not supported on Windows\n";
                        return false;
                    } else {
                        std::wcerr << L"Unknown flag option: -" << c << L"\nUse -h for help.\n";
                        return false;
                    }
                }
            } else {
                std::wcerr << L"Unexpected argument: " << arg << L"\nUse -h for help.\n";
                return false;
            }
        }

        if (opts.showScanTime && argc != 2) {
            std::wcerr << L"ioscan: -t cannot be combined with other options\n";
            return false;
        }
        if (opts.showAlias && !opts.agileView) {
            std::wcerr << L"ioscan: -A requires -N\n";
            return false;
        }
        if (!opts.propertyName.empty() && !opts.agileView) {
            std::wcerr << L"ioscan: -P requires -N\n";
            return false;
        }
        return true;
    }
};

class IoscanApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, wchar_t* argv[]) {
        ConsoleManager::ConfigureNativePipes();

        IoscanOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        std::vector<DeviceRecord> devices;
        std::unordered_map<std::wstring, int> summaryCount;

        if (!DeviceEnumerator::Enumerate(opts, devices, summaryCount)) {
            return 1;
        }

        return OutputFormatter::RenderOutput(opts, devices, summaryCount);
    }
};
