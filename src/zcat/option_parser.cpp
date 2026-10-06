#include "option_parser.hpp"
#include "zcat_options.hpp"

void OptionParser::PrintUsage(const wchar_t* progName) {
        std::wcout
            << L"Usage: " << progName << L" file.gz...\n"
            << L"Write decompressed gzip file content to standard output.\n\n"
            << L"Options:\n"
            << L"      --json         Emit decompressed content as JSON\n"
            << L"      --csv          Emit decompressed content as CSV\n"
            << L"      --table        Emit decompressed content as a table\n"
            << L"      --pipe COMMAND Send output through COMMAND\n"
            << L"  -h, --help     Show this help\n"
            << L"      --version  Show version\n";
    }

void OptionParser::PrintVersion() {
        std::wcout << L"zcat 1.0.0\n";
    }

bool OptionParser::Parse(int argc, wchar_t* argv[], ZcatOptions& opts) const {
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i] ? argv[i] : L"";
            if (arg == L"--json") {
                opts.format = OutputFormat::Json;
            } else if (arg == L"--csv") {
                opts.format = OutputFormat::Csv;
            } else if (arg == L"--table") {
                opts.format = OutputFormat::Table;
            } else if (arg == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i] ? argv[i] : L"";
            } else if (arg == L"-h" || arg == L"--help") {
                opts.showHelp = true;
                return true;
            } else if (arg == L"--version") {
                opts.showVersion = true;
                return true;
            } else if (!arg.empty() && arg[0] == L'-') {
                std::wcerr << L"zcat: unknown option -- " << arg << L"\n";
                return false;
            } else {
                opts.files.push_back(arg);
            }
        }
        return true;
    }
