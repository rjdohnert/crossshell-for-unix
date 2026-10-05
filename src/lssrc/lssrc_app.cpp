#include "lssrc_app.hpp"
#include "options.hpp"
#include "engine.hpp"
#include "reporter.hpp"
#include <iostream>
#include <algorithm>
#include <cctype>

int LssrcApplication::Run(int argc, wchar_t* argv[]) {
    SetConsoleOutputCP(CP_UTF8);

    LssrcOptions opts;
    if (!opts.Parse(argc, argv)) {
        std::cerr << "Try 'lssrc --help' for usage.\n";
        return 2;
    }

    if (opts.showHelp) {
        opts.PrintUsage("lssrc");
        return 0;
    }

    if (opts.showVersion) {
        opts.PrintVersion();
        return 0;
    }

    std::vector<ServiceRow> allRows;
    std::string errorMsg;
    if (!ServiceManagerEngine::EnumerateServices(opts.includeStopped, allRows, errorMsg)) {
        std::cerr << "lssrc: " << errorMsg << "\n";
        return 1;
    }

    std::wstring match = ServiceManagerEngine::ToLower(opts.specificService);
    std::vector<ServiceRow> filteredRows;

    for (const auto& row : allRows) {
        std::wstring wName(row.name.begin(), row.name.end());
        if (!match.empty() && ServiceManagerEngine::ToLower(wName) != match) {
            continue;
        }

        bool selected = opts.filters.empty();
        std::string lowerName = row.name;
        std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });

        for (const auto& filter : opts.filters) {
            std::string lowerFilter = ServiceManagerEngine::ToUtf8(ServiceManagerEngine::ToLower(filter));
            if (lowerName.find(lowerFilter) != std::string::npos) {
                selected = true;
                break;
            }
        }

        if (selected) {
            filteredRows.push_back(row);
        }
    }

    if (!match.empty() && filteredRows.empty()) {
        std::wcerr << L"lssrc: service not found: " << opts.specificService << L"\n";
        return 1;
    }

    LssrcReporter::Emit(opts, filteredRows);
    return 0;
}
