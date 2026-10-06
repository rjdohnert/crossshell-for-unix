#include "uname_options.hpp"

void UnameOptions::printHelp() {
        std::wcout << LR"(UNAME(1)                         User Commands                        UNAME(1)

NAME
    uname v3.1.0 - print or set system information

SYNOPSIS
     uname [-a | -s | -n | -r | -v | -m | -i | -l | -M | -X]
     uname -S nodename

OPTIONS
     -a, --all        Print all core system identification fields in canonical order.
     -s, --sysname    Print the system implementation name (Windows NT).
     -n, --nodename   Print the network node hostname.
     -r, --release    Print the operating system release level (e.g., 10.0.24H2).
     -v, --version    Print the operating system kernel version and build number.
     -m, --machine    Print the hardware machine instruction architecture (e.g., x86_64, arm64).
     -i, --id         Print the hardware machine identifier (Machine GUID / UUID).
     -l, --license    Print the system software license / Windows Product ID number.
     -M, --model      Print the hardware system model and motherboard name.
     -X               Print extended system summary in SVr4 / Solaris format.
     -S nodename      Set the network node hostname (Requires Administrator elevation).
         --json, --csv, --table  Structured output.
         --pipe CMD   Send output through CMD.
     -h, --help       Display this comprehensive help section and exit.
)";
    }

void UnameOptions::printVersionHeader() {
        std::wcout << L"uname 3.1.0\n";
    }

bool UnameOptions::parse(int argc, wchar_t* argv[], UnameOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                printHelp();
                std::exit(0);
            } else if (arg == L"--version") {
                printVersionHeader();
                std::exit(0);
            } else if (arg == L"--json") {
                opts.outputFormat = 1;
            } else if (arg == L"--csv") {
                opts.outputFormat = 2;
            } else if (arg == L"--table") {
                opts.outputFormat = 3;
            } else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg == L"-a" || arg == L"--all") {
                opts.printSysname = opts.printNodename = opts.printRelease = true;
                opts.printVersion = opts.printMachine = opts.printId = true;
            } else if (arg == L"-s" || arg == L"--sysname" || arg == L"--kernel-name") {
                opts.printSysname = true;
            } else if (arg == L"-n" || arg == L"--nodename") {
                opts.printNodename = true;
            } else if (arg == L"-r" || arg == L"--release" || arg == L"--kernel-release") {
                opts.printRelease = true;
            } else if (arg == L"-v" || arg == L"--kernel-version") {
                opts.printVersion = true;
            } else if (arg == L"-m" || arg == L"--machine" || arg == L"-p" || arg == L"--processor") {
                opts.printMachine = true;
            } else if (arg == L"-i" || arg == L"--id") {
                opts.printId = true;
            } else if (arg == L"-l" || arg == L"--license") {
                opts.printLicense = true;
            } else if (arg == L"-M" || arg == L"--model") {
                opts.printModel = true;
            } else if (arg == L"-X") {
                opts.printExtended = true;
            } else if (arg == L"-S" && i + 1 < argc) {
                opts.setHostname = argv[++i];
            } else if (arg[0] == L'-' && arg.size() > 1) {
                for (size_t j = 1; j < arg.size(); ++j) {
                    wchar_t c = arg[j];
                    if (c == L'a') { opts.printSysname = opts.printNodename = opts.printRelease = opts.printVersion = opts.printMachine = opts.printId = true; }
                    else if (c == L's') opts.printSysname = true;
                    else if (c == L'n') opts.printNodename = true;
                    else if (c == L'r') opts.printRelease = true;
                    else if (c == L'v') opts.printVersion = true;
                    else if (c == L'm' || c == L'p') opts.printMachine = true;
                    else if (c == L'i') opts.printId = true;
                    else if (c == L'l') opts.printLicense = true;
                    else if (c == L'M') opts.printModel = true;
                    else if (c == L'X') opts.printExtended = true;
                    else {
                        std::wcerr << L"uname: unknown option -- " << c << L"\n";
                        return false;
                    }
                }
            } else {
                std::wcerr << L"uname: extra operand " << arg << L"\n";
                return false;
            }
        }

        if (!opts.printSysname && !opts.printNodename && !opts.printRelease &&
            !opts.printVersion && !opts.printMachine && !opts.printId &&
            !opts.printLicense && !opts.printModel && !opts.printExtended && opts.setHostname.empty()) {
            opts.printSysname = true;
        }

        return true;
    }
