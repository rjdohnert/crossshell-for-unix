#include "options.hpp"

bool NodenameOptions::Parse(int argc, wchar_t* argv[]) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i] ? argv[i] : L"";
        if (arg == L"--") {
            for (int j = i + 1; j < argc; ++j) {
                if (!hasNewHostname) {
                    newHostname = argv[j] ? argv[j] : L"";
                    hasNewHostname = true;
                } else {
                    std::wcerr << L"nodename: too many arguments\n";
                    return false;
                }
            }
            break;
        } else if (arg == L"-h" || arg == L"--help") {
            showHelp = true;
            return true;
        } else if (arg == L"--version" || arg == L"-V") {
            showVersion = true;
            return true;
        } else if (arg == L"-s" || arg == L"--short") {
            mode = NameQueryMode::ShortName;
        } else if (arg == L"-d" || arg == L"--domain") {
            mode = NameQueryMode::DomainOnly;
        } else if (arg == L"-f" || arg == L"--fqdn") {
            mode = NameQueryMode::FullyQualified;
        } else if (arg.size() > 1 && arg[0] == L'-') {
            for (size_t j = 1; j < arg.size(); ++j) {
                switch (arg[j]) {
                    case L's':
                        mode = NameQueryMode::ShortName;
                        break;
                    case L'd':
                        mode = NameQueryMode::DomainOnly;
                        break;
                    case L'f':
                        mode = NameQueryMode::FullyQualified;
                        break;
                    default:
                        std::wcerr << L"nodename: unknown option -- " << arg[j] << L"\n";
                        return false;
                }
            }
        } else {
            if (hasNewHostname) {
                std::wcerr << L"nodename: too many arguments\n";
                return false;
            }
            newHostname = arg;
            hasNewHostname = true;
        }
    }
    return true;
}

void NodenameOptions::PrintUsage(const wchar_t* progName) const {
    std::wcout << L"Usage: " << (progName ? progName : L"nodename") << L" [-f] [-s | -d] [name-of-host]\n"
               << L"  -s, --short       print the short host name\n"
               << L"  -d, --domain      print the DNS domain name\n"
               << L"  -f, --fqdn        print the fully qualified domain name\n"
               << L"  -h, --help        display this help and exit\n"
               << L"      --version     output version information and exit\n"
               << L"      --            end of options\n";
}

void NodenameOptions::PrintVersion() const {
    std::wcout << L"nodename 1.0.0\n";
}
