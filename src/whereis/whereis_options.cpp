#include "whereis_options.hpp"

void WhereisOptions::printUsage(const wchar_t* progName) {
        std::wcout << L"Usage: " << progName << L" [-bms] [-u] [-BMS directory... -f] name...\n\n"
                   << L"Locate the binary, source, and manual-page files for a command.\n\n"
                   << L"Options:\n"
                   << L"  -b          Search only for binaries\n"
                   << L"  -m          Search only for manual/documentation sections\n"
                   << L"  -s          Search only for sources\n"
                   << L"  -u          Search only for unusual entries (entries without all requested parts)\n"
                   << L"  -B <dir>    Set/change search directory list for binaries\n"
                   << L"  -M <dir>    Set/change search directory list for manuals\n"
                   << L"  -S <dir>    Set/change search directory list for sources\n"
                   << L"  -f          Terminates directory lists and begins file list\n"
                   << L"      --json, --csv, --table  structured output\n"
                   << L"      --pipe CMD  send output through CMD\n"
                   << L"  -h, --help  Display this help message and exit\n";
    }

bool WhereisOptions::parse(int argc, wchar_t* argv[], WhereisOptions& opts) {
        opts.config.initializeDefaults();
        enum TargetList { NONE, BIN, MAN, SRC } currentDirList = NONE;

        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
                printUsage(argv[0]);
                std::exit(0);
            }
            if (arg == L"--json") { opts.outputFormat = 1; continue; }
            if (arg == L"--csv") { opts.outputFormat = 2; continue; }
            if (arg == L"--table") { opts.outputFormat = 3; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { opts.pipeCommand = argv[++i]; continue; }

            if (arg == L"-b") { opts.config.searchBin = true; opts.config.searchMan = false; opts.config.searchSrc = false; currentDirList = NONE; }
            else if (arg == L"-m") { opts.config.searchBin = false; opts.config.searchMan = true; opts.config.searchSrc = false; currentDirList = NONE; }
            else if (arg == L"-s") { opts.config.searchBin = false; opts.config.searchMan = false; opts.config.searchSrc = true; currentDirList = NONE; }
            else if (arg == L"-B") { opts.config.binDirs.clear(); currentDirList = BIN; }
            else if (arg == L"-M") { opts.config.manDirs.clear(); currentDirList = MAN; }
            else if (arg == L"-S") { opts.config.srcDirs.clear(); currentDirList = SRC; }
            else if (arg == L"-f") { currentDirList = NONE; }
            else {
                if (currentDirList == BIN) opts.config.binDirs.push_back(arg);
                else if (currentDirList == MAN) opts.config.manDirs.push_back(arg);
                else if (currentDirList == SRC) opts.config.srcDirs.push_back(arg);
                else opts.targets.push_back(arg);
            }
        }

        if (opts.targets.empty()) {
            printUsage(argv[0]);
            return false;
        }

        return true;
    }
