#include "setfacl_options.hpp"

void SetfaclOptionParser::PrintUsage(const wchar_t* progName) {
    std::wcout
        << L"Usage: " << progName << L" [OPTIONS] ACL_SPEC FILE...\n"
        << L"Apply a simple access control entry to Windows files.\n\n"
        << L"ACL syntax (baseline):\n"
        << L"  u:NAME:rwx   user entry\n"
        << L"  g:NAME:rwx   group entry\n"
        << L"  o::rwx       other/everyone entry\n\n"
        << L"Options:\n"
        << L"  -m, --modify   apply an ACL entry (default)\n"
        << L"  -b, --remove   remove the DACL (baseline clear)\n"
        << L"  -R, --recursive operate recursively on directories\n"
        << L"  -h, --help    display this help and exit\n"
        << L"  -V, --version output version information and exit\n"
        << L"  --            end of options\n";
}

void SetfaclOptionParser::PrintVersion() {
    std::wcout << L"setfacl 1.0.0\n";
}

bool SetfaclOptionParser::Parse(int argc, wchar_t* argv[], SetfaclOptions& opts) const {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";
        if (arg == L"--") {
            for (++i; i < argc; ++i) {
                opts.paths.push_back(argv[i] ? argv[i] : L"");
            }
            break;
        }
        if (arg == L"-h" || arg == L"--help") {
            opts.showHelp = true;
            return true;
        }
        if (arg == L"-V" || arg == L"--version") {
            opts.showVersion = true;
            return true;
        }
        if (arg == L"-R" || arg == L"--recursive") {
            opts.recursive = true;
            continue;
        }
        if (arg == L"-b" || arg == L"--remove") {
            opts.removeDacl = true;
            continue;
        }
        if (arg == L"-m" || arg == L"--modify") {
            continue;
        }
        if (opts.spec.empty() && !opts.removeDacl) {
            opts.spec = arg;
            continue;
        }
        opts.paths.push_back(arg);
    }

    if ((opts.spec.empty() && !opts.removeDacl) || opts.paths.empty()) {
        return false;
    }

    return true;
}
