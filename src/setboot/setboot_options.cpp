#include "setboot_options.hpp"

void SetbootOptions::printHelp(const wchar_t* progName) {
    std::wcout << L"Boot Environment Parameter v3.1.0\n"
               << L"Usage: " << progName << L" [-p id] [-a id] [-b on|off] [-t seconds] [-v] [-h]\n\n"
               << L"OPTIONS:\n"
               << L"  -p id          Set Primary Boot Path target in NVRAM BootOrder (e.g., Boot0001 or 1).\n"
               << L"  -a id          Set Alternate / Next Boot Path target (sets NVRAM BootNext).\n"
               << L"  -b on|off      Enable or disable Autoboot delay (sets timeout to 5s or 0s).\n"
               << L"  -t seconds     Set Autoboot delay timeout in seconds.\n"
               << L"  -v, --verbose  Display extended NVRAM variable attributes and EFI paths.\n"
               << L"  -h, /?         Display this comprehensive help section.\n\n"
               << L"PRIVILEGES:\n"
               << L"  Modifying NVRAM variables (-p, -a, -b, -t) requires Administrator rights\n"
               << L"  and SeSystemEnvironmentPrivilege enabled.\n\n"
               << L"EXAMPLES:\n"
               << L"  " << progName << L"\n"
               << L"  " << progName << L" -v\n"
               << L"  " << progName << L" -p Boot0001\n"
               << L"  " << progName << L" -a Boot0002\n"
               << L"  " << progName << L" -t 10\n"
               << L"  " << progName << L" -b off\n";
}

std::wstring SetbootOptions::toUpper(std::wstring str) {
    std::transform(str.begin(), str.end(), str.begin(), ::towupper);
    return str;
}

bool SetbootOptions::parse(int argc, wchar_t* argv[], SetbootOptions& opts) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
            printHelp(argv[0]);
            std::exit(0);
        } else if (arg == L"-v" || arg == L"--verbose") {
            opts.verbose = true;
        } else if (arg == L"-p" && i + 1 < argc) {
            opts.setPrimary = argv[++i];
            opts.isModify = true;
        } else if (arg == L"-a" && i + 1 < argc) {
            opts.setAlternate = argv[++i];
            opts.isModify = true;
        } else if (arg == L"-b" && i + 1 < argc) {
            opts.setAutoboot = toUpper(argv[++i]);
            opts.isModify = true;
        } else if (arg == L"-t" && i + 1 < argc) {
            opts.setTimeout = _wtoi(argv[++i]);
            opts.isModify = true;
        } else {
            std::wcerr << L"Unknown option: " << arg << L"\nUse -h for help.\n";
            return false;
        }
    }
    return true;
}
