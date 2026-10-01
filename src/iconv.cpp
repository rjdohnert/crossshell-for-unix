/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors may be
 *    used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * ============================================================================
 * SINGLE FILE INDEX: iconv.cpp
 * ============================================================================
 * WinIconv - Object-Oriented Character Set Converter for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CODEPAGE REGISTRY] ......... IconvOptions and EncodingRegistry classes
 * 2. [STRUCTURED OUTPUT REPORTER] .......... IconvReporter class (JSON/CSV/Table/Pipe)
 * 3. [CHARSET CONVERSION ENGINE] ........... CharsetConverter and IconvEngine classes
 * 4. [APPLICATION CONTROLLER] .............. IconvApp class and main entry point
 * ============================================================================
 */

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <fcntl.h>
#include <io.h>

#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <memory>

// ============================================================================
// 1. OPTIONS & CODEPAGE REGISTRY
// ============================================================================

constexpr UINT CP_PSEUDO_UTF16LE = 1200;
constexpr UINT CP_PSEUDO_UTF16BE = 1201;
constexpr UINT CP_PSEUDO_UTF32LE = 12000;
constexpr UINT CP_PSEUDO_UTF32BE = 12001;

class IconvOptions {
public:
    std::string fromCode;
    std::string toCode;
    std::string outputFile;
    std::vector<std::string> inputFiles;
    bool discardInvalid{false}; // -c / //IGNORE
    bool translit{false};        // //TRANSLIT
    bool silent{false};          // -s
    bool list{false};            // -l
    int outputFormat{0};
    std::string pipeCommand;

    static void printHelp() {
        std::cout << R"(iconv(1)            CrossShell for UNIX Reference Manual                 iconv(1)

    NAME
        iconv - convert text character encoding

    SYNOPSIS
        iconv [OPTIONS] [-f FROM_ENCODING] [-t TO_ENCODING] [FILE...]
        iconv -l

    DESCRIPTION
        iconv converts text from one character set encoding to another. If no
        input files are given, or if FILE is '-', standard input is read. Output
        is written to standard output unless specified otherwise by -o.

    OPTIONS
        -f, --from-code FROM
            Specify the input character encoding.

        -t, --to-code TO
            Specify the output character encoding.

        -c
            Discard characters that cannot be converted instead of terminating.

        -s, --silent
            Suppress conversion error warning messages.

        -l, --list
            List all supported character set encodings.

        -o, --output FILE
            Write converted output to FILE instead of standard output.

        --json
            Output results formatted as JSON.

        --csv
            Output results formatted as CSV.

        --table
            Output results formatted as a table.

        --pipe COMMAND
            Send output through the specified pipe command.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        iconv -f UTF-8 -t UTF-16 input.txt -o output.txt
            Convert input.txt from UTF-8 to UTF-16 and write to output.txt.

        iconv -l
            List all supported character encodings.

        type file.txt | iconv -f ISO-8859-1 -t UTF-8
            Convert standard input stream to UTF-8.

    CrossShell for UNIX                                                    iconv(1)
)";
    }

    static void printVersion() {
        std::cout << "iconv v2.2.0\n";
    }

    static bool parse(int argc, char* argv[], IconvOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h" || arg == "/?") {
                printHelp();
                std::exit(0);
            } else if (arg == "--version" || arg == "-V") {
                printVersion();
                std::exit(0);
            } else if (arg == "-l" || arg == "--list") {
                opts.list = true;
            } else if (arg == "-c") {
                opts.discardInvalid = true;
            } else if (arg == "-s" || arg == "--silent") {
                opts.silent = true;
            } else if (arg.rfind("-f", 0) == 0) {
                opts.fromCode = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "");
            } else if (arg.rfind("--from-code=", 0) == 0) {
                opts.fromCode = arg.substr(12);
            } else if (arg.rfind("-t", 0) == 0) {
                opts.toCode = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "");
            } else if (arg.rfind("--to-code=", 0) == 0) {
                opts.toCode = arg.substr(10);
            } else if (arg.rfind("-o", 0) == 0) {
                opts.outputFile = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "");
            } else if (arg.rfind("--output=", 0) == 0) {
                opts.outputFile = arg.substr(9);
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg[0] == '-' && arg.size() > 1) {
                std::cerr << "iconv: unrecognized option '" << arg << "'\n";
                return false;
            } else {
                opts.inputFiles.push_back(arg);
            }
        }

        if (opts.inputFiles.empty() && !opts.list) {
            opts.inputFiles.push_back("-");
        }

        return true;
    }
};

class EncodingRegistry {
public:
    static std::string normalize(std::string_view name) {
        std::string norm;
        norm.reserve(name.size());
        for (char c : name) {
            if (c != '-' && c != '_' && c != ' ' && c != ':') {
                norm.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            }
        }
        return norm;
    }

    static UINT lookupCodePage(const std::string& name) {
        static const std::unordered_map<std::string, UINT> registry = {
            {"utf8", CP_UTF8}, {"utf8bom", CP_UTF8}, {"utf7", CP_UTF7},
            {"utf16", CP_PSEUDO_UTF16LE}, {"utf16le", CP_PSEUDO_UTF16LE}, {"utf16be", CP_PSEUDO_UTF16BE},
            {"ucs2", CP_PSEUDO_UTF16LE}, {"ucs2le", CP_PSEUDO_UTF16LE}, {"ucs2be", CP_PSEUDO_UTF16BE},
            {"utf32", CP_PSEUDO_UTF32LE}, {"utf32le", CP_PSEUDO_UTF32LE}, {"utf32be", CP_PSEUDO_UTF32BE},
            {"ascii", 20127}, {"usascii", 20127},
            {"latin1", 28591}, {"iso88591", 28591}, {"iso88592", 28592}, {"iso885915", 28605},
            {"windows1250", 1250}, {"cp1250", 1250},
            {"windows1251", 1251}, {"cp1251", 1251},
            {"windows1252", 1252}, {"cp1252", 1252},
            {"shiftjis", 932}, {"sjis", 932}, {"cp932", 932},
            {"gbk", 936}, {"cp936", 936}, {"gb2312", 936},
            {"euckr", 949}, {"cp949", 949},
            {"big5", 950}, {"cp950", 950}
        };

        std::string n = normalize(name);
        auto it = registry.find(n);
        if (it != registry.end()) return it->second;

        // Try direct codepage integer
        try {
            return static_cast<UINT>(std::stoul(name));
        } catch (...) {
            return 0;
        }
    }

    static void listSupported() {
        std::cout << "The following encodings are supported:\n"
                  << "  UTF-8, UTF-16, UTF-16LE, UTF-16BE, UTF-32, UTF-32LE, UTF-32BE\n"
                  << "  ASCII, ISO-8859-1, ISO-8859-2, ISO-8859-15\n"
                  << "  CP1250, CP1251, CP1252 (Windows ANSI)\n"
                  << "  Shift-JIS (CP932), GBK/GB2312 (CP936), EUC-KR (CP949), Big5 (CP950)\n";
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class IconvReporter {
public:
    static int dispatch(const std::string& content, int format, const std::string& pipeCommand, std::ostream& directOut) {
        if (format != 0 || !pipeCommand.empty()) {
            std::string text;
            if (format == 1) {
                text = "{\"data\":\"" + content + "\"}\n";
            } else if (format == 2) {
                text = "data\n\"" + content + "\"\n";
            } else if (format == 3) {
                text = "DATA\n----\n" + content + "\n";
            } else {
                text = content;
            }

            if (!pipeCommand.empty()) {
                FILE* pipe = _popen(pipeCommand.c_str(), "w");
                if (!pipe) return 1;
                std::fwrite(text.data(), 1, text.size(), pipe);
                _pclose(pipe);
            } else {
                directOut << text;
            }
        } else {
            directOut << content;
        }
        return 0;
    }
};

// ============================================================================
// 3. CHARSET CONVERSION ENGINE
// ============================================================================

class CharsetConverter {
public:
    static bool convert(const std::string& input, UINT fromCp, UINT toCp, std::string& output, bool discardInvalid) {
        if (fromCp == toCp) {
            output = input;
            return true;
        }

        // Convert FromCP -> UTF-16 wstring
        int wLen = MultiByteToWideChar(fromCp, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);
        if (wLen <= 0) {
            if (discardInvalid) { output = input; return true; }
            return false;
        }

        std::wstring wide(wLen, L'\0');
        MultiByteToWideChar(fromCp, 0, input.data(), static_cast<int>(input.size()), &wide[0], wLen);

        // Convert UTF-16 wstring -> ToCP
        int outLen = WideCharToMultiByte(toCp, 0, wide.data(), wLen, nullptr, 0, nullptr, nullptr);
        if (outLen <= 0) {
            if (discardInvalid) return true;
            return false;
        }

        output.resize(outLen);
        WideCharToMultiByte(toCp, 0, wide.data(), wLen, &output[0], outLen, nullptr, nullptr);
        return true;
    }
};

class IconvEngine {
private:
    IconvOptions options;

public:
    explicit IconvEngine(IconvOptions opts) : options(std::move(opts)) {}

    int execute() {
        if (options.list) {
            EncodingRegistry::listSupported();
            return 0;
        }

        UINT fromCp = EncodingRegistry::lookupCodePage(options.fromCode.empty() ? "utf8" : options.fromCode);
        UINT toCp = EncodingRegistry::lookupCodePage(options.toCode.empty() ? "utf8" : options.toCode);

        if (fromCp == 0) {
            std::cerr << "iconv: conversion from " << options.fromCode << " is not supported\n";
            return 1;
        }
        if (toCp == 0) {
            std::cerr << "iconv: conversion to " << options.toCode << " is not supported\n";
            return 1;
        }

#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif

        std::ostream* out = &std::cout;
        std::ofstream fileOut;
        if (!options.outputFile.empty()) {
            fileOut.open(options.outputFile, std::ios::binary);
            if (!fileOut.is_open()) {
                std::cerr << "iconv: cannot open '" << options.outputFile << "' for writing\n";
                return 1;
            }
            out = &fileOut;
        }

        for (const auto& path : options.inputFiles) {
            std::istream* in = &std::cin;
            std::ifstream fileIn;
            if (path != "-") {
                fileIn.open(path, std::ios::binary);
                if (!fileIn.is_open()) {
                    if (!options.silent) std::cerr << "iconv: cannot open '" << path << "'\n";
                    return 1;
                }
                in = &fileIn;
            }

            std::string content((std::istreambuf_iterator<char>(*in)), std::istreambuf_iterator<char>());
            std::string converted;

            if (!CharsetConverter::convert(content, fromCp, toCp, converted, options.discardInvalid)) {
                if (!options.silent) std::cerr << "iconv: conversion failed on " << path << "\n";
                return 1;
            }

            IconvReporter::dispatch(converted, options.outputFormat, options.pipeCommand, *out);
        }

        return 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class IconvApp {
public:
    static int run(int argc, char* argv[]) {
        IconvOptions options;
        if (!IconvOptions::parse(argc, argv, options)) {
            return 1;
        }
        IconvEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return IconvApp::run(argc, argv);
}