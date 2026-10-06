#include "options.hpp"

bool NiceOptions::Parse(int argc, wchar_t* argv[]) {
    if (argc == 1) {
        return true;
    }

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];
        if (arg == L"--") {
            cmdIndex = i + 1;
            break;
        } else if ((arg == L"-n" || arg == L"--adjustment") && i + 1 < argc) {
            niceIncrement = std::wcstol(argv[++i], nullptr, 10);
        } else if (arg.rfind(L"-n", 0) == 0 && arg.length() > 2) {
            niceIncrement = std::wcstol(arg.substr(2).c_str(), nullptr, 10);
        } else if (arg.rfind(L"-", 0) == 0 && arg.length() > 1 && std::iswdigit(arg[1])) {
            niceIncrement = std::wcstol(arg.c_str(), nullptr, 10);
        } else if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
            showHelp = true;
            return true;
        } else if (arg == L"--version") {
            showVersion = true;
            return true;
        } else {
            cmdIndex = i;
            break;
        }
    }
    return true;
}

void NiceOptions::PrintUsage(const wchar_t* exe) const {
    std::wcout << L"nice\n";
    std::wcout << L"Usage: " << exe << L" [-n increment] [COMMAND [ARGS...]]\n\n";
    std::wcout << L"Options:\n";
    std::wcout << L"  -n, --adjustment=N   Add N to the niceness (default: 10)\n";
    std::wcout << L"  -h, --help           Show this help message\n";
    std::wcout << L"      --version        Show version information\n";
    std::wcout << L"      --               End of options\n\n";
    std::wcout << L"Niceness Range (-20 to 19) Mapping:\n";
    std::wcout << L"  -20 to -15 : REALTIME_PRIORITY_CLASS\n";
    std::wcout << L"  -14 to -5  : HIGH_PRIORITY_CLASS\n";
    std::wcout << L"  -4  to -1  : ABOVENORMAL_PRIORITY_CLASS\n";
    std::wcout << L"   0         : NORMAL_PRIORITY_CLASS\n";
    std::wcout << L"   1  to 10  : BELOWNORMAL_PRIORITY_CLASS (Default when -n is omitted)\n";
    std::wcout << L"  11  to 19  : IDLE_PRIORITY_CLASS\n\n";
    std::wcout << L"Examples:\n";
    std::wcout << L"  nice                             (Print current niceness)\n";
    std::wcout << L"  nice myapp.exe                   (Run with +10 niceness / Below Normal)\n";
    std::wcout << L"  nice -n 19 ffmpeg.exe -i in out  (Run with low priority)\n";
    std::wcout << L"  nice -n -10 powershell.exe       (Run with high priority)\n";
}

void NiceOptions::PrintVersion() const {
    std::wcout << L"nice v1.0.0\n";
}
