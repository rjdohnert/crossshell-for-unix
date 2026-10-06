#include "buffer_config.hpp"
#include "stdbuf_options.hpp"

void StdbufOptions::printHelp() {
        std::wcout << LR"(stdbuf(1)               CrossShell for UNIX Reference Manual                 stdbuf(1)

    NAME
        stdbuf - run command with modified buffering operations for standard streams

    SYNOPSIS
        stdbuf [OPTIONS] COMMAND [ARGUMENTS...]

    DESCRIPTION
        Runs COMMAND, modifying buffering operations for its standard streams
        (standard input, standard output, and standard error). MODE can be '0'
        for unbuffered, 'L' for line-buffered, or an explicit byte size for
        block buffering.

    OPTIONS
        -i, --input=MODE
            Adjust standard input stream buffering.

        -o, --output=MODE
            Adjust standard output stream buffering.

        -e, --error=MODE
            Adjust standard error stream buffering.

        -h, --help
            Display this reference manual.

        -v, -V, --version
            Display version and license information.

    EXAMPLES
        stdbuf -o0 tail -f log.txt
            Run tail -f with unbuffered standard output.

        stdbuf -iL -oL my_filter
            Run my_filter with line-buffered input and output streams.

        stdbuf -o 8192 producer | consumer
            Run producer with an 8 KB output buffer.

    CrossShell for UNIX                                                    stdbuf(1)
    )";
    }

void StdbufOptions::printVersion() {
        std::wcout << L"stdbuf 1.0\n";
    }

bool StdbufOptions::parse(int argc, wchar_t* argv[], StdbufOptions& opts) {
        int i = 1;
        for (; i < argc; ++i) {
            std::wstring arg = argv[i];
            if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
                printHelp();
                std::exit(0);
            } else if (arg == L"--version" || arg == L"-V") {
                printVersion();
                std::exit(0);
            } else if (arg.rfind(L"-i", 0) == 0) {
                std::wstring val = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : L"L");
                opts.inConfig = BufferConfig::parse(val);
                opts.inSet = true;
            } else if (arg.rfind(L"--input=", 0) == 0) {
                opts.inConfig = BufferConfig::parse(arg.substr(8));
                opts.inSet = true;
            } else if (arg.rfind(L"-o", 0) == 0) {
                std::wstring val = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : L"L");
                opts.outConfig = BufferConfig::parse(val);
                opts.outSet = true;
            } else if (arg.rfind(L"--output=", 0) == 0) {
                opts.outConfig = BufferConfig::parse(arg.substr(9));
                opts.outSet = true;
            } else if (arg.rfind(L"-e", 0) == 0) {
                std::wstring val = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : L"L");
                opts.errConfig = BufferConfig::parse(val);
                opts.errSet = true;
            } else if (arg.rfind(L"--error=", 0) == 0) {
                opts.errConfig = BufferConfig::parse(arg.substr(8));
                opts.errSet = true;
            } else if (arg[0] == L'-') {
                std::wcerr << L"stdbuf: unrecognized option: " << arg << L"\n";
                return false;
            } else {
                break;
            }
        }

        for (; i < argc; ++i) {
            opts.commandArgs.push_back(argv[i]);
        }

        if (opts.commandArgs.empty()) {
            std::wcerr << L"stdbuf: missing operand\n";
            std::wcerr << L"Try 'stdbuf --help' for more information.\n";
            return false;
        }

        return true;
    }
