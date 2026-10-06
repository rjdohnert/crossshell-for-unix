#include "option_parser.hpp"
#include "tail_options.hpp"

void OptionParser::PrintUsage() {
        std::wcout << LR"(tail(1)                 CrossShell for UNIX Reference Manual                  tail(1)

    NAME
        tail - output the last part of files

    SYNOPSIS
        tail [OPTIONS] [FILE]...

    DESCRIPTION
        Print the last 10 lines of each FILE to standard output. With more than
        one FILE, precede each with a header giving the file name. With no FILE,
        or when FILE is '-', read standard input.

    OPTIONS
        -c, --bytes=[+]NUM
            Output the last NUM bytes; or use +NUM to output starting with byte NUM.

        -f, --follow
            Output appended data as the file grows.

        -n, --lines=[+]NUM
            Output the last NUM lines, instead of the last 10; or use +NUM to start at line NUM.

        --max-unchanged-stats=N
            Reopen file after N unchanged checks with -f (default: 5).

        -q, --quiet, --silent
            Never output headers giving file names.

        --retry
            Keep trying to open a file if it is inaccessible.

        -s, --sleep-interval=N
            With -f, sleep approximately N seconds (default: 1.0) between iterations.

        -v, --verbose
            Always output headers giving file names.

        -h, --help
            Display this reference manual.

        --version
            Display version and license information.

    EXAMPLES
        tail app.log
            Print the last 10 lines of app.log.

        tail -n 25 error.log
            Print the last 25 lines of error.log.

        tail -f -n 50 /var/log/syslog
            Follow syslog in real-time starting from last 50 lines.

        tail -n +100 dump.sql
            Output dump.sql starting from line 100 to end of file.

    CrossShell for UNIX                                                     tail(1)
    )";
    }

void OptionParser::PrintVersion() {
        std::wcout << L"tail (cmd-extended) 2.0.0\n"
                   << L"Copyright (C) 2026 Free Software Foundation, Inc.\n";
    }

bool OptionParser::ParseCount(const std::wstring& str, long long& count, bool& from_start) {
        if (str.empty()) return false;
        size_t idx = 0;
        if (str[0] == L'+') {
            from_start = true;
            idx = 1;
        } else if (str[0] == L'-') {
            from_start = false;
            idx = 1;
        } else {
            from_start = false;
        }

        try {
            count = std::stoll(str.substr(idx));
            return true;
        } catch (...) {
            return false;
        }
    }

bool OptionParser::Parse(int argc, wchar_t* argv[], TailOptions& opts, bool& exitEarly) const {
        exitEarly = false;
        for (int i = 1; i < argc; ++i) {
            std::wstring arg = argv[i];

            if (arg == L"--help" || arg == L"-h" || arg == L"-?" || arg == L"/?") {
                PrintUsage();
                exitEarly = true;
                return true;
            } else if (arg == L"--version") {
                PrintVersion();
                exitEarly = true;
                return true;
            } else if (arg == L"-f" || arg == L"--follow") {
                opts.follow = true;
            } else if (arg == L"--retry") {
                opts.retry = true;
            } else if (arg == L"-q" || arg == L"--quiet" || arg == L"--silent") {
                opts.quiet = true;
                opts.verbose = false;
            } else if (arg == L"-v" || arg == L"--verbose") {
                opts.verbose = true;
                opts.quiet = false;
            } else if (arg.rfind(L"-n", 0) == 0) {
                opts.count_lines = true;
                if (arg == L"-n") {
                    if (i + 1 < argc) {
                        if (!ParseCount(argv[++i], opts.count, opts.from_start)) {
                            std::wcerr << L"tail: invalid number of lines: '" << argv[i] << L"'\n";
                            return false;
                        }
                    }
                } else {
                    if (!ParseCount(arg.substr(2), opts.count, opts.from_start)) {
                        std::wcerr << L"tail: invalid number of lines: '" << arg.substr(2) << L"'\n";
                        return false;
                    }
                }
            } else if (arg.rfind(L"-c", 0) == 0) {
                opts.count_lines = false;
                if (arg == L"-c") {
                    if (i + 1 < argc) {
                        if (!ParseCount(argv[++i], opts.count, opts.from_start)) {
                            std::wcerr << L"tail: invalid number of bytes: '" << argv[i] << L"'\n";
                            return false;
                        }
                    }
                } else {
                    if (!ParseCount(arg.substr(2), opts.count, opts.from_start)) {
                        std::wcerr << L"tail: invalid number of bytes: '" << arg.substr(2) << L"'\n";
                        return false;
                    }
                }
            } else if (arg.rfind(L"-s", 0) == 0) {
                if (arg == L"-s") {
                    if (i + 1 < argc) opts.sleep_interval_sec = std::stod(argv[++i]);
                } else {
                    opts.sleep_interval_sec = std::stod(arg.substr(2));
                }
            } else if (arg.size() > 1 && (arg[0] == L'+' || (arg[0] == L'-' && std::isdigit(static_cast<unsigned char>(arg[1]))))) {
                opts.count_lines = true;
                ParseCount(arg, opts.count, opts.from_start);
            } else if (!arg.empty() && arg[0] == L'-' && arg != L"-") {
                std::wcerr << L"tail: unrecognized option '" << arg << L"'\n";
                return false;
            } else {
                opts.files.push_back(arg);
            }
        }

        if (opts.files.empty()) {
            opts.files.push_back(L"-");
        }

        return true;
    }
