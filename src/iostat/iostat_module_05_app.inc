// ============================================================================
// 5. OPTION PARSER & APPLICATION CONTROLLER
// ============================================================================

class OptionParser {
public:
    bool Parse(int argc, char* argv[], IostatOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--") {
                for (++i; i < argc; ++i) {
                    if (!opts.saw_interval) {
                        try { opts.interval = std::stoi(argv[i]); if (opts.interval < 1) opts.interval = 1; opts.saw_interval = true; }
                        catch (...) { std::cerr << "iostat: invalid interval: " << argv[i] << "\n"; return false; }
                    } else if (!opts.saw_count) {
                        try { opts.count = std::stoi(argv[i]); if (opts.count < 1) opts.count = 1; opts.saw_count = true; }
                        catch (...) { std::cerr << "iostat: invalid count: " << argv[i] << "\n"; return false; }
                    } else {
                        std::cerr << "iostat: too many arguments\n";
                        return false;
                    }
                }
                break;
            }

            if (arg == "-h" || arg == "--help") {
                OutputFormatter::PrintUsage();
                exitEarly = true;
                return true;
            }
            if (arg == "-V" || arg == "--version") {
                OutputFormatter::PrintVersion();
                exitEarly = true;
                return true;
            }
            if (arg == "-k" || arg == "--kilobytes") {
                opts.scale = 1024.0;
                opts.unit = "kB";
            } else if (arg == "-m" || arg == "--megabytes") {
                opts.scale = 1024.0 * 1024.0;
                opts.unit = "MB";
            } else if (arg == "-n" || arg == "--interval") {
                if (i + 1 >= argc) {
                    std::cerr << "iostat: option requires an argument -- n\n";
                    return false;
                }
                try { opts.interval = std::stoi(argv[++i]); if (opts.interval < 1) opts.interval = 1; opts.saw_interval = true; }
                catch (...) { std::cerr << "iostat: invalid interval: " << argv[i] << "\n"; return false; }
            } else if (arg == "-c" || arg == "--count") {
                if (i + 1 >= argc) {
                    std::cerr << "iostat: option requires an argument -- c\n";
                    return false;
                }
                try { opts.count = std::stoi(argv[++i]); if (opts.count < 1) opts.count = 1; opts.saw_count = true; }
                catch (...) { std::cerr << "iostat: invalid count: " << argv[i] << "\n"; return false; }
            } else if (arg[0] == '-') {
                std::cerr << "iostat: unrecognized option: " << arg << "\n";
                OutputFormatter::PrintUsage();
                return false;
            } else {
                if (!opts.saw_interval) {
                    try { opts.interval = std::stoi(arg); if (opts.interval < 1) opts.interval = 1; opts.saw_interval = true; }
                    catch (...) { std::cerr << "iostat: invalid interval: " << arg << "\n"; return false; }
                } else if (!opts.saw_count) {
                    try { opts.count = std::stoi(arg); if (opts.count < 1) opts.count = 1; opts.saw_count = true; }
                    catch (...) { std::cerr << "iostat: invalid count: " << arg << "\n"; return false; }
                } else {
                    std::cerr << "iostat: too many arguments\n";
                    return false;
                }
            }
        }
        return true;
    }
};

class IostatApplication {
private:
    OptionParser m_parser;
    PdhCounterEngine m_engine;

public:
    int Run(int argc, char* argv[]) {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        IostatOptions opts;
        bool exitEarly = false;
        if (!m_parser.Parse(argc, argv, opts, exitEarly)) {
            return 1;
        }
        if (exitEarly) {
            return 0;
        }

        if (!m_engine.Initialize()) {
            return 1;
        }

        std::cout << "\n";
        std::cout << "Collecting metrics... Use Ctrl+C to terminate.\n";

        int iterations = 0;
        while (true) {
            if (opts.count != -1 && iterations >= opts.count) {
                break;
            }

            std::this_thread::sleep_for(std::chrono::seconds(opts.interval == 0 ? 1 : opts.interval));

            double userPct = 0.0, sysPct = 0.0, idlePct = 0.0;
            std::map<std::wstring, DiskStats> diskMap;

            if (!m_engine.CollectSample(userPct, sysPct, idlePct, diskMap)) {
                break;
            }

            OutputFormatter::PrintCpuMetrics(userPct, sysPct, idlePct);
            OutputFormatter::PrintDiskTable(diskMap, opts.scale);

            iterations++;
            if (opts.interval == 0) {
                break;
            }
        }

        return 0;
    }
};
