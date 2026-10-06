#include "options.hpp"

void OptionParser::PrintUsage() {
    std::cout << "Usage: pbcopy [-pboard {general | ruler | find | font}] [-h | --help] [-v | --version]\n"
              << "Read standard input and place it into the Windows system clipboard.\n\n"
              << "Options:\n"
              << "  -pboard <name>   Specify pasteboard (general, ruler, find, font).\n"
              << "                   (On Windows, all options map to system clipboard).\n"
              << "  -h, --help       Display this help menu.\n"
              << "  -v, --version    Display version information.\n"
              << "  --               End of options; read from standard input.\n"
              << "  -                Treat as stdin placeholder (no-op).\n";
}

void OptionParser::PrintVersion() {
    std::cout << "pbcopy version 1.0\n";
}

bool OptionParser::Parse(int argc, wchar_t* argv[], PbcopyOptions& opts, bool& exitEarly) const {
    exitEarly = false;
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"-h" || arg == L"--help" || arg == L"-help") {
            PrintUsage();
            exitEarly = true;
            return true;
        } else if (arg == L"-v" || arg == L"--version") {
            PrintVersion();
            exitEarly = true;
            return true;
        } else if (arg == L"--") {
            break;
        } else if (arg == L"-") {
            continue;
        } else if (arg == L"-pboard") {
            if (i + 1 < argc) {
                opts.pasteboard = argv[++i];
            } else {
                std::cerr << "pbcopy: Error: -pboard option requires an argument.\n";
                return false;
            }
        } else {
            std::wcerr << L"pbcopy: Invalid option: " << arg << L"\n";
            PrintUsage();
            return false;
        }
    }
    return true;
}
