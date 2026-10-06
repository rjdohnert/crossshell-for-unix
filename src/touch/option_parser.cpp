#include "option_parser.hpp"
#include "touch_options.hpp"

void OptionParser::PrintUsage() {
        std::wcout << LR"(touch(1)                CrossShell for UNIX Reference Manual                 touch(1)

    NAME
        touch - change file timestamps or create empty files

    SYNOPSIS
        touch [OPTIONS] FILE...

    DESCRIPTION
        Update the access and modification times of each FILE to the current
        time. A FILE argument that does not exist is created empty, unless -c
        is supplied.
        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -a
            Change only the access time.

        -c, --no-create
            Do not create any files.

        -m
            Change only the modification time.

        -r FILE, --reference=FILE
            Use this file's times instead of current time.

        -t TIME
            Use [[CC]YY]MMDDhhmm[.ss] instead of current time.

        --date=STRING
            Parse STRING and use it as the date.

        --json, --csv, --table
            Select structured output format.

        --pipe COMMAND
            Send output through pipeline COMMAND.

        -h, --help
            Display this reference manual and exit.

        -v, --version
            Display version information and exit.

    EXAMPLES
        touch file.txt
            Update timestamp of file.txt, creating it if it does not exist.

        touch -c non_existing.txt
            Do not create file if it does not exist.

        touch -r source.txt target.txt
            Set target.txt timestamps to match source.txt.

    CrossShell for UNIX                                                      touch(1)
)";
    }

bool OptionParser::Parse(int argc, wchar_t* argv[], TouchOptions& opts) const {
        int i = 1;
        while (i < argc) {
            std::wstring arg = argv[i];
            if (arg == L"--json") { opts.output_format = 1; i++; continue; }
            if (arg == L"--csv") { opts.output_format = 2; i++; continue; }
            if (arg == L"--table") { opts.output_format = 3; i++; continue; }
            if (arg == L"--pipe" && i + 1 < argc) { opts.pipe_command = argv[++i]; i++; continue; }
            if (arg == L"-h" || arg == L"--help" || arg == L"/?" || arg == L"-?") {
                PrintUsage();
                return false;
            }
            if (arg == L"-v" || arg == L"--version") {
                std::wcout << L"touch version 1.0.0\n";
                return false;
            }
            if (arg.rfind(L"--date=", 0) == 0) {
                opts.date_str = arg.substr(7);
            } else if (arg.rfind(L"--reference=", 0) == 0) {
                opts.ref_file = arg.substr(12);
            } else if (arg.rfind(L"--time=", 0) == 0) {
                opts.time_str = arg.substr(7);
            } else if (arg[0] == L'-' && arg.length() > 1) {
                for (size_t j = 1; j < arg.length(); ++j) {
                    wchar_t flag = arg[j];
                    if (flag == L'a') {
                        opts.change_access = true;
                    } else if (flag == L'c') {
                        opts.no_create = true;
                    } else if (flag == L'm') {
                        opts.change_mod = true;
                    } else if (flag == L'r') {
                        if (j + 1 == arg.length()) {
                            if (i + 1 < argc) {
                                opts.ref_file = argv[++i];
                                break;
                            } else {
                                std::wcerr << L"touch: option requires an argument -- r\n";
                                return false;
                            }
                        } else {
                            opts.ref_file = arg.substr(j + 1);
                            break;
                        }
                    } else if (flag == L't') {
                        if (j + 1 == arg.length()) {
                            if (i + 1 < argc) {
                                opts.time_str = argv[++i];
                                break;
                            } else {
                                std::wcerr << L"touch: option requires an argument -- t\n";
                                return false;
                            }
                        } else {
                            opts.time_str = arg.substr(j + 1);
                            break;
                        }
                    } else {
                        std::wcerr << L"touch: unknown option -- " << flag << std::endl;
                        std::wcerr << L"usage: touch [-acm] [-r file] [-t time] file ...\n";
                        return false;
                    }
                }
            } else {
                opts.targets.push_back(arg);
            }
            i++;
        }

        if (opts.targets.empty()) {
            std::wcerr << L"usage: touch [-acm] [-r file] [-t time] file ...\n";
            return false;
        }

        return true;
    }
