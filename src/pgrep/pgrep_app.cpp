#include "pgrep_app.hpp"

int PgrepApplication::Run(int argc, wchar_t* argv[]) {
    PgrepOptions options;
    if (!m_parser.Parse(argc, argv, options)) {
        OptionParser::PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"pgrep");
        return 2;
    }

    if (options.showHelp) {
        OptionParser::PrintUsage((argc > 0 && argv[0]) ? argv[0] : L"pgrep");
        return 0;
    }

    if (options.showVersion) {
        OptionParser::PrintVersion();
        return 0;
    }

    std::vector<ProcessRecord> processes;
    if (!ProcessSnapshot::Collect(processes)) {
        std::wcerr << L"pgrep: failed to enumerate processes\n";
        return 2;
    }

    std::vector<ProcessRecord> matches;
    for (const auto& p : processes) {
        if (ProcessMatcher::IsMatch(p.name, options)) {
            matches.push_back(p);
        }
    }

    if (options.countOnly) {
        std::wcout << matches.size() << L"\n";
        return matches.empty() ? 1 : 0;
    }

    for (const auto& p : matches) {
        if (options.listName) {
            std::wcout << p.pid << L" " << p.name << L"\n";
        } else {
            std::wcout << p.pid << L"\n";
        }
    }

    return matches.empty() ? 1 : 0;
}
