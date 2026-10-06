#include "options.hpp"

PsConfig CommandLineParser::parse(int argc, wchar_t* argv[]) {
    PsConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
            cfg.showHelp = true;
            return cfg;
        } else if (arg == L"-e" || arg == L"-A" || arg == L"-a") {
            cfg.showAll = true;
        } else if (arg == L"-f") {
            cfg.fullFormat = true;
        } else if (arg == L"-l") {
            cfg.longFormat = true;
        } else if (arg == L"--json") {
            cfg.mode = OutputMode::JSON;
        } else if (arg == L"--csv") {
            cfg.mode = OutputMode::CSV;
        } else if (arg == L"--xml") {
            cfg.mode = OutputMode::XML;
        } else if (arg == L"--table") {
            cfg.mode = OutputMode::TABLE;
        } else if (arg == L"--output" && i + 1 < argc) {
            std::wstring val = argv[++i];
            if (val == L"json") cfg.mode = OutputMode::JSON;
            else if (val == L"csv") cfg.mode = OutputMode::CSV;
            else if (val == L"xml") cfg.mode = OutputMode::XML;
            else if (val == L"table") cfg.mode = OutputMode::TABLE;
            else cfg.parseError = true;
        } else if (arg.rfind(L"--output=", 0) == 0) {
            std::wstring val = arg.substr(9);
            if (val == L"json") cfg.mode = OutputMode::JSON;
            else if (val == L"csv") cfg.mode = OutputMode::CSV;
            else if (val == L"xml") cfg.mode = OutputMode::XML;
            else if (val == L"table") cfg.mode = OutputMode::TABLE;
            else cfg.parseError = true;
        } else if ((arg == L"-u" || arg == L"--user") && i + 1 < argc) {
            cfg.userFilter = argv[++i];
        } else if ((arg == L"-p" || arg == L"-q" || arg == L"--pid") && i + 1 < argc) {
            std::wstringstream ss(argv[++i]);
            std::wstring token;
            while (std::getline(ss, token, L',')) {
                try { cfg.pidFilter.insert(std::stoul(token)); } catch (...) {}
            }
        } else if ((arg == L"-o" || arg == L"--format") && i + 1 < argc) {
            std::wstringstream ss(argv[++i]);
            std::wstring token;
            while (std::getline(ss, token, L',')) {
                token.erase(0, token.find_first_not_of(L" \t"));
                token.erase(token.find_last_not_of(L" \t") + 1);
                if (!token.empty()) cfg.customColumns.push_back(token);
            }
        }
    }
    return cfg;
}
