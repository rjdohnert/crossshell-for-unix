#include "option_parser.hpp"
#include "zgrep_options.hpp"

void OptionParser::PrintUsage(const wchar_t* progName) {
        std::wcout
            << L"Usage: " << progName << L" [grep-options] pattern file.gz...\n"
            << L"Search compressed gzip files using grep semantics (baseline mode).\n\n"
            << L"Notes:\n"
            << L"  - Gzip files are decompressed to temporary files before grep runs.\n"
            << L"  - stdin mode is not supported in this baseline.\n"
            << L"  - --json, --csv, and --table format grep output records.\n"
            << L"  - --pipe COMMAND sends output through COMMAND.\n"
            << L"  -h, --help     Show this help\n"
            << L"      --version  Show version\n";
    }

void OptionParser::PrintVersion() {
        std::wcout << L"zgrep 1.0.0\n";
    }

bool OptionParser::Parse(int argc, wchar_t* argv[], ZgrepOptions& opts) const {
        for (int i = 1; i < argc; ++i) {
            std::wstring a = argv[i] ? argv[i] : L"";
            if (a == L"-h" || a == L"--help") {
                opts.showHelp = true;
                return true;
            }
            if (a == L"--version") {
                opts.showVersion = true;
                return true;
            }
            if (a == L"--json") {
                opts.format = OutputFormat::Json;
            } else if (a == L"--csv") {
                opts.format = OutputFormat::Csv;
            } else if (a == L"--table") {
                opts.format = OutputFormat::Table;
            } else if (a == L"--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i] ? argv[i] : L"";
            } else {
                opts.rawArgs.push_back(a);
            }
        }
        return true;
    }
