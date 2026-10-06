#include "options.hpp"

void PrtconfOptions::printHelp() {
    std::cout << R"(prtconf(1)                CrossShell for UNIX Reference Manual                prtconf(1)

    NAME
        prtconf - print system hardware and configuration summary

    SYNOPSIS
        prtconf [OPTIONS]

    DESCRIPTION
        prtconf displays system configuration information including host model,
        node name, processor architecture, CPU count, total installed memory,
        operating system build, and hardware device counts.

    OPTIONS
        -v
            Verbose mode; display device count breakdown grouped by setup class.

        -h, --help
            Display this reference manual and exit.

        -V, --version
            Display version information and exit.

    EXAMPLES
        prtconf
            Print standard system configuration summary.

        prtconf -v
            Print configuration summary with verbose device-class breakdown.

    CrossShell for UNIX                                                          prtconf(1)
)";
}

void PrtconfOptions::printVersion() {
    std::cout << "prtconf 1.0.0\n";
}

bool PrtconfOptions::parse(int argc, wchar_t* argv[], PrtconfOptions& opt) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";
        if (arg == L"-v") {
            opt.showDevices = true;
        } else if (arg == L"-h" || arg == L"--help" || arg == L"/?") {
            printHelp();
            std::exit(0);
        } else if (arg == L"-V" || arg == L"--version") {
            printVersion();
            std::exit(0);
        } else {
            std::wcerr << L"prtconf: unknown option '" << arg << L"'\n";
            return false;
        }
    }
    return true;
}
