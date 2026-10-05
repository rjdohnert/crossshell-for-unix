bool parse_args(int argc, wchar_t* argv[], Options& opt) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";
        if (arg == L"-h" || arg == L"--help") {
            opt.help = true;
            return true;
        }
        if (arg == L"-v" || arg == L"--version") {
            opt.version = true;
            return true;
        }
        if (arg == L"-a" || arg == L"--all") {
            opt.showAll = true;
            continue;
        }
        if ((arg == L"-l" || arg == L"--log") && i + 1 < argc) {
            std::wstring log = resolve_log(argv[++i]);
            if (log.empty()) {
                std::wcerr << L"errpt: unsupported log '" << argv[i] << L"'\n";
                return false;
            }
            opt.logName = log;
            continue;
        }
        if ((arg == L"-n" || arg == L"--lines") && i + 1 < argc) {
            wchar_t* end = nullptr;
            unsigned long v = wcstoul(argv[++i], &end, 10);
            if (end == argv[i] || *end != L'\0' || v == 0) {
                std::wcerr << L"errpt: invalid count '" << argv[i] << L"'\n";
                return false;
            }
            opt.maxEntries = static_cast<DWORD>(v);
            continue;
        }
        if (arg == L"--json" || arg == L"--output=json") {
            opt.output = Options::OutputFormat::Json;
            continue;
        }
        if (arg == L"--csv" || arg == L"--output=csv") {
            opt.output = Options::OutputFormat::Csv;
            continue;
        }
        if (arg == L"--pipe" && i + 1 < argc) { opt.pipeCommand = argv[++i]; continue; }
        if (arg == L"--table" || arg == L"--output=table") {
            opt.output = Options::OutputFormat::Table;
            continue;
        }
        if (arg == L"--output" && i + 1 < argc) {
            std::wstring mode = to_lower(argv[++i] ? argv[i] : L"");
            if (mode == L"json") opt.output = Options::OutputFormat::Json;
            else if (mode == L"csv") opt.output = Options::OutputFormat::Csv;
            else if (mode == L"table") opt.output = Options::OutputFormat::Table;
            else {
                std::wcerr << L"errpt: unsupported output mode '" << mode << L"'\n";
                return false;
            }
            continue;
        }

        std::wcerr << L"errpt: unknown option '" << arg << L"'\n";
        return false;
    }
    return true;
}
