#include "option_parser.hpp"
#include "swlist_options.hpp"
#include "system_info.hpp"

bool OptionParser::IsValidLevel(const wstring& level) {
        static const set<wstring> kValidLevels = {
            L"PRODUCT", L"BUNDLE", L"VENDOR", L"ALL"
        };
        return kValidLevels.find(level) != kValidLevels.end();
    }

bool OptionParser::IsValidAttribute(const wstring& attribute) {
        static const set<wstring> kValidAttributes = {
            L"ALL", L"TITLE", L"REVISION", L"VERSION", L"VENDOR", L"PUBLISHER",
            L"DATE", L"LOCATION", L"PATH", L"ARCH", L"ARCHITECTURE"
        };
        return kValidAttributes.find(attribute) != kValidAttributes.end();
    }

void OptionParser::PrintHelp(const wchar_t* progName) {
        wcout << L"Software Distributor Inspector v9.2.0\n"
              << L"Usage: " << progName << L" [-l level] [-a attribute] [-v] [pattern ...]\n\n"
              << L"OPTIONS:\n"
              << L"  -l level       Specify the detail level of software listing:\n"
              << L"                   product   List top-level installed products (default).\n"
              << L"                   bundle    Alias for product view for compatibility.\n"
              << L"                   vendor    Group products by vendor/publisher.\n"
              << L"                   all       Display all software including system components.\n"
              << L"  -a attribute   Display a specific software attribute:\n"
              << L"                   title     Display product name only.\n"
              << L"                   revision  Display product name and version.\n"
              << L"                   vendor    Display product name and vendor.\n"
              << L"                   date      Display product name and install date.\n"
              << L"                   location  Display product name and install path.\n"
              << L"                   arch      Display product name and architecture (x64/x86/User).\n"
              << L"  -v, -f         Verbose full view showing revision, vendor, date, arch, and path.\n"
              << L"  -h, /?         Display this comprehensive help documentation.\n\n"
              << L"OPERANDS:\n"
              << L"  pattern        Case-insensitive filter string matching software names or vendors.\n\n"
              << L"EXAMPLES:\n"
              << L"  " << progName << L"\n"
              << L"  " << progName << L" -v\n"
              << L"  " << progName << L" -a revision\n"
              << L"  " << progName << L" -a vendor Python\n"
              << L"  " << progName << L" -l vendor Microsoft\n"
              << L"  " << progName << L" Microsoft\n";
        wcout << L"  --json, --csv, --table  Select output format\n"
              << L"  --pipe COMMAND          Send output through COMMAND\n";
    }

bool OptionParser::Parse(int argc, wchar_t* argv[], SwlistOptions& opts) const {
        for (int i = 1; i < argc; ++i) {
            wstring arg = argv[i];

            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                opts.showHelp = true;
                return true;
            } else if (arg == L"-v" || arg == L"-f" || arg == L"--verbose") {
                opts.verbose = true;
            } else if (arg == L"-l") {
                if (i + 1 < argc) {
                    opts.level = SystemInfo::ToUpper(argv[++i]);
                } else {
                    wcerr << L"Error: -l option requires a level argument.\n";
                    return false;
                }
            } else if (arg == L"-a") {
                if (i + 1 < argc) {
                    opts.attribute = SystemInfo::ToUpper(argv[++i]);
                } else {
                    wcerr << L"Error: -a option requires an attribute argument.\n";
                    return false;
                }
            } else if (arg == L"--json") {
                opts.output_format = 1;
            } else if (arg == L"--csv") {
                opts.output_format = 2;
            } else if (arg == L"--table") {
                opts.output_format = 3;
            } else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipe_command = argv[++i];
            } else if (arg.length() > 1 && (arg[0] == L'-' || arg[0] == L'/')) {
                for (size_t j = 1; j < arg.length(); ++j) {
                    wchar_t c = arg[j];
                    if (c == L'v' || c == L'f') opts.verbose = true;
                    else if (c == L'l') {
                        if (j + 1 < arg.length()) {
                            opts.level = SystemInfo::ToUpper(arg.substr(j + 1));
                            break;
                        } else if (i + 1 < argc) {
                            opts.level = SystemInfo::ToUpper(argv[++i]);
                            break;
                        } else {
                            wcerr << L"Error: -l option requires a level argument.\n";
                            return false;
                        }
                    } else if (c == L'a') {
                        if (j + 1 < arg.length()) {
                            opts.attribute = SystemInfo::ToUpper(arg.substr(j + 1));
                            break;
                        } else if (i + 1 < argc) {
                            opts.attribute = SystemInfo::ToUpper(argv[++i]);
                            break;
                        } else {
                            wcerr << L"Error: -a option requires an attribute argument.\n";
                            return false;
                        }
                    } else {
                        wcerr << L"Unknown flag option: -" << c << L"\nUse -h for help.\n";
                        return false;
                    }
                }
            } else {
                opts.searchPatterns.push_back(arg);
            }
        }

        if (!IsValidLevel(opts.level)) {
            wcerr << L"Unknown level: " << opts.level << L"\nUse -h for help.\n";
            return false;
        }

        if (!IsValidAttribute(opts.attribute)) {
            wcerr << L"Unknown attribute: " << opts.attribute << L"\nUse -h for help.\n";
            return false;
        }

        if (opts.level == L"BUNDLE") {
            opts.level = L"PRODUCT";
        }

        if (opts.level == L"ALL") {
            opts.showSystemComponents = true;
        }

        return true;
    }
