#include "option_parser.hpp"
#include "tee_options.hpp"

void OptionParser::PrintHelp() {
        std::wcout << LR"(tee(1)                  CrossShell for UNIX Reference Manual                  tee(1)

    NAME
        tee - duplicate standard input to files and standard output

    SYNOPSIS
        tee [OPTIONS] [FILE]...

    DESCRIPTION
        Copy standard input to each FILE, and also to standard output.
        Supports continuous binary stream multiplexing across multiple file targets
        and console pipes.

    OPTIONS
        -a, --append
            Append to the given files, do not overwrite.

        -i, --ignore-interrupts
            Ignore interrupt signals (SIGINT / Ctrl+C).

        --json
            Emit input capture telemetry in JSON format.

        --csv
            Emit input capture telemetry in CSV format.

        --table
            Emit input capture telemetry in tabular format.

        --pipe COMMAND
            Stream captured output into COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Display version and license information.

    EXAMPLES
        ls -la | tee output.txt
            Display directory listing and write it to output.txt.

        make 2>&1 | tee -a build.log
            Append build output to build.log while viewing in console.

        cat data.csv | tee file1.csv file2.csv file3.csv
            Duplicate stream across multiple destination files simultaneously.

    CrossShell for UNIX                                                     tee(1)
    )";
    }

void OptionParser::PrintVersion() {
        std::wcout << L"tee\n";
    }

bool OptionParser::Parse(int argc, wchar_t* argv[], TeeOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--help" || arg == L"-h" || arg == L"-?" || arg == L"/?") {
                PrintHelp();
                exitEarly = true;
                return true;
            }
            if (arg == L"--version") {
                PrintVersion();
                exitEarly = true;
                return true;
            }
            if (arg == L"--append") {
                opts.append = true;
                continue;
            }
            if (arg == L"--ignore-interrupts") {
                opts.ignore_interrupts = true;
                continue;
            }
            if (arg == L"--json") { opts.output_format = 1; continue; }
            if (arg == L"--csv") { opts.output_format = 2; continue; }
            if (arg == L"--table") { opts.output_format = 3; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { opts.pipe_command = argv[++i]; continue; }
            if (arg[0] == L'-' && arg.size() > 1) {
                for (size_t j = 1; j < arg.size(); ++j) {
                    switch (arg[j]) {
                        case L'a':
                            opts.append = true;
                            break;
                        case L'i':
                            opts.ignore_interrupts = true;
                            break;
                        default:
                            std::wcerr << L"tee: unknown option -- " << arg[j] << std::endl;
                            std::wcerr << L"usage: tee [-ai] [file ...]\n";
                            return false;
                    }
                }
            } else {
                opts.file_paths.push_back(arg);
            }
        }
        return true;
    }
