static int errpt_main(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    Options opt;
    if (!parse_args(argc, argv, opt)) {
        std::cerr << "Try 'errpt --help' for usage.\n";
        return 2;
    }
    if (opt.help) {
        print_help();
        return 0;
    }
    if (opt.version) {
        print_version();
        return 0;
    }

    ErrptOutputGuard outputGuard(opt.pipeCommand);

    HANDLE hLog = OpenEventLogW(nullptr, opt.logName.c_str());
    if (!hLog) {
        std::wcerr << L"errpt: cannot open log '" << opt.logName << L"' (error " << GetLastError() << L")\n";
        return 1;
    }

    constexpr DWORD kBufferSize = 1 << 16;
    std::vector<BYTE> buffer(kBufferSize);
    DWORD bytesRead = 0;
    DWORD minBytes = 0;
    DWORD printed = 0;

    std::vector<EventEntry> entries;

    DWORD flags = EVENTLOG_BACKWARDS_READ | EVENTLOG_SEQUENTIAL_READ;
    while (ReadEventLogW(hLog, flags, 0, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, &minBytes)) {
        DWORD offset = 0;
        while (offset < bytesRead) {
            const EVENTLOGRECORD* rec = reinterpret_cast<const EVENTLOGRECORD*>(buffer.data() + offset);
            const wchar_t* sourceW = reinterpret_cast<const wchar_t*>(reinterpret_cast<const BYTE*>(rec) + sizeof(EVENTLOGRECORD));
            std::string source = to_utf8(sourceW ? sourceW : L"");
            std::string summary = first_insertion_string(rec);
            if (summary.empty()) {
                summary = "(no insertion text)";
            }

            entries.push_back({ format_time(rec->TimeGenerated), severity_word(rec->EventType),
                                rec->EventID & 0xFFFF, std::move(source), std::move(summary) });

            ++printed;
            if (!opt.showAll && printed >= opt.maxEntries) {
                CloseEventLog(hLog);
                print_entries(entries, opt.output);
                return 0;
            }

            offset += rec->Length;
        }
    }

    DWORD err = GetLastError();
    CloseEventLog(hLog);

    if (err != ERROR_HANDLE_EOF && err != ERROR_SUCCESS) {
        std::cerr << "errpt: failed while reading log (error " << err << ")\n";
        return 1;
    }

    print_entries(entries, opt.output);
    return 0;
}
