#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "options.hpp"
#include <iostream>

static std::string utf8_from_wide(const std::wstring& text) {
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

void CommandLineParser::printVersion() {
    std::cout << "lex 2.6.4 \n";
    std::cout << "Copyright (c) 2026, Roberto J Dohnert\n";
}

void CommandLineParser::printHelp() {
    std::cout << R"(lex(1)              CrossShell for UNIX Reference Manual                 lex(1)

    NAME
        lex - generate lexical analyzers from specifications

    SYNOPSIS
        lex [OPTIONS] [FILE...]

    DESCRIPTION
        lex generates C or C++ lexical analyzer scanner source code from lexical
        specifications. When no FILE is specified, or when FILE is '-', the
        specification is read from standard input.

    OPTIONS
        -o, --outfile=FILE
            Specify scanner output filename (default: lex.yy.c or lex.yy.cc).

        -t, --stdout
            Write generated scanner to standard output.

        --header-file=FILE
            Generate an additional C/C++ header file.

        -+, --c++
            Generate a C++ scanner class.

        -i, --case-insensitive
            Generate a case-insensitive scanner.

        -s, --nodefault
            Suppress default rule to echo unmatched text.

        -P, --prefix=PREFIX
            Use PREFIX instead of default 'yy'.

        -d, --debug
            Enable debug mode in generated scanner.

        -v, --verbose
            Write summary scanner statistics to stderr.

        -h, -?, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        lex lexer.l
            Generate lex.yy.c from lexer.l.

        lex -+ -o scanner.cpp lexer.l
            Generate C++ scanner class in scanner.cpp.

        type lexer.l | lex -t > lex.yy.c
            Process specification from standard input stream.

        lex --header-file=scanner.h -o scanner.c lexer.l
            Generate scanner and header file together.

    CrossShell for UNIX                                                    lex(1)
)";
}

FlexOptions CommandLineParser::parse(int argc, wchar_t* argv[]) {
    FlexOptions opts;

    for (int i = 1; i < argc; ++i) {
        std::wstring warg = argv[i];
        std::string arg = utf8_from_wide(warg);

        if (arg == "-h" || arg == "-?" || arg == "--help" || arg == "/?") {
            opts.showHelp = true;
            return opts;
        }
        if (arg == "-V" || arg == "--version") {
            opts.showVersion = true;
            return opts;
        }
        if (arg == "-t" || arg == "--stdout") {
            opts.stdoutMode = true;
        } else if (arg == "-+" || arg == "--c++") {
            opts.generateCpp = true;
        } else if (arg == "-i" || arg == "--case-insensitive") {
            opts.caseInsensitive = true;
        } else if (arg == "-s" || arg == "--nodefault") {
            opts.suppressDefault = true;
        } else if (arg == "-d" || arg == "--debug") {
            opts.debugMode = true;
        } else if (arg == "-v" || arg == "--verbose") {
            opts.verbose = true;
        } else if (arg.rfind("-o", 0) == 0 || arg.rfind("--outfile", 0) == 0) {
            if (arg.rfind("--outfile=", 0) == 0) opts.outputFile = arg.substr(10);
            else if (arg == "-o" && i + 1 < argc) {
                std::wstring wNext = argv[++i];
                opts.outputFile = utf8_from_wide(wNext);
            } else if (arg.rfind("-o", 0) == 0 && arg.length() > 2) {
                opts.outputFile = arg.substr(2);
            }
        } else if (arg.rfind("-P", 0) == 0 || arg.rfind("--prefix", 0) == 0) {
            if (arg.rfind("--prefix=", 0) == 0) opts.prefix = arg.substr(9);
            else if (arg == "-P" && i + 1 < argc) {
                std::wstring wNext = argv[++i];
                opts.prefix = utf8_from_wide(wNext);
            } else if (arg.rfind("-P", 0) == 0 && arg.length() > 2) {
                opts.prefix = arg.substr(2);
            }
        } else if (arg.rfind("--header-file=", 0) == 0) {
            opts.headerFile = arg.substr(14);
        } else if (!arg.empty() && arg.front() != '-') {
            opts.inputFilePath = arg;
        }
    }

    // Set default output file name if not writing to stdout
    if (opts.outputFile.empty() && !opts.stdoutMode) {
        opts.outputFile = opts.generateCpp ? "lex.yy.cc" : "lex.yy.c";
    }

    return opts;
}
