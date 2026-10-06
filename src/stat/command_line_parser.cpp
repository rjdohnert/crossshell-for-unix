#include "command_line_parser.hpp"
#include "stat_options.hpp"

ProgramOptions CommandLineParser::parse(int argc, wchar_t* argv[]) {
        ProgramOptions opts;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"-?" || arg == L"/?" || arg == L"-h" || arg == L"--help") {
                opts.showHelp = true;
            } else if (arg == L"-v" || arg == L"-V" || arg == L"--version") {
                opts.showVersion = true;
            } else if (arg == L"-t" || arg == L"--table") {
                opts.format = OutputFormat::Table;
            } else if (arg == L"-d" || arg == L"--detailed") {
                opts.format = OutputFormat::Detailed;
            } else if (arg == L"--json") {
                opts.format = OutputFormat::JSON;
            } else if (arg == L"--csv") {
                opts.format = OutputFormat::CSV;
            } else if (arg == L"-b" || arg == L"--bytes") {
                opts.humanReadable = false;
            } else if (arg.rfind(L"-", 0) != 0) {
                opts.targetPaths.push_back(arg);
            }
        }
        return opts;
    }
