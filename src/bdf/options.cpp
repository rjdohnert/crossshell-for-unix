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
 *
 * CrossShell for UNIX
 */

#include "options.hpp"
#include <cstdlib>
#include <iostream>

ProgramOptions CommandLineParser::parse(int argc, wchar_t* argv[]) {
    ProgramOptions opts;

    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        if (arg == L"-h" || arg == L"--human-readable") {
            opts.scale = ScaleUnit::HumanBinary;
        } else if (arg == L"-H" || arg == L"--si") {
            opts.scale = ScaleUnit::HumanDecimal;
        } else if (arg == L"-k") {
            opts.scale = ScaleUnit::KiloBinary;
        } else if (arg == L"-m") {
            opts.scale = ScaleUnit::MegaBinary;
        } else if (arg == L"-g" || arg == L"-G") {
            opts.scale = ScaleUnit::GigaBinary;
        } else if (arg == L"-a" || arg == L"--all") {
            opts.showAll = true;
        } else if (arg == L"-l" || arg == L"--local") {
            opts.showLocalOnly = true;
        } else if (arg == L"-T" || arg == L"--print-type") {
            opts.printType = true;
        } else if (arg == L"--total") {
            opts.printTotal = true;
        } else if (arg == L"--help" || arg == L"-?") {
            opts.showHelp = true;
        } else if (arg == L"-v" || arg == L"--version") {
            opts.showVersion = true;
        } else if (arg.rfind(L"-B", 0) == 0 || arg.rfind(L"--block-size", 0) == 0) {
            opts.scale = ScaleUnit::CustomBlock;
            std::wstring val;
            if (arg.rfind(L"-B", 0) == 0 && arg.length() > 2) {
                val = arg.substr(2);
            } else if (arg.rfind(L"--block-size=", 0) == 0) {
                val = arg.substr(13);
            } else if (i + 1 < argc) {
                val = argv[++i];
            }
            opts.customBlockSize = parseBlockSize(val);
        } else if (arg.rfind(L"-t", 0) == 0 || arg.rfind(L"--type", 0) == 0) {
            if (arg.rfind(L"--type=", 0) == 0) {
                opts.filterFsType = arg.substr(7);
            } else if (i + 1 < argc) {
                opts.filterFsType = argv[++i];
            }
        } else if (arg.rfind(L"-x", 0) == 0 || arg.rfind(L"--exclude-type", 0) == 0) {
            if (arg.rfind(L"--exclude-type=", 0) == 0) {
                opts.excludeFsType = arg.substr(15);
            } else if (i + 1 < argc) {
                opts.excludeFsType = argv[++i];
            }
        } else if (arg.length() > 0 && arg[0] == L'-' && arg.length() > 1 && arg[1] != L'-') {
            for (size_t c = 1; c < arg.length(); ++c) {
                switch (arg[c]) {
                    case L'h': opts.scale = ScaleUnit::HumanBinary; break;
                    case L'H': opts.scale = ScaleUnit::HumanDecimal; break;
                    case L'k': opts.scale = ScaleUnit::KiloBinary; break;
                    case L'm': opts.scale = ScaleUnit::MegaBinary; break;
                    case L'a': opts.showAll = true; break;
                    case L'l': opts.showLocalOnly = true; break;
                    case L'T': opts.printType = true; break;
                    case L'?': opts.showHelp = true; break;
                    default:
                        std::wcerr << L"bdf: unrecognized option -- '" << arg[c] << L"'\n";
                        std::wcerr << L"Try 'bdf --help' or 'bdf -?' for more information.\n";
                        std::exit(1);
                }
            }
        } else {
            opts.targetPaths.push_back(arg);
        }
    }
    return opts;
}

uint64_t CommandLineParser::parseBlockSize(const std::wstring& val) {
    if (val.empty()) return 1024;
    try {
        size_t idx = 0;
        uint64_t size = std::stoull(val, &idx);
        if (idx < val.length()) {
            wchar_t unit = val[idx];
            switch (unit) {
                case L'K': case L'k': size *= 1024ULL; break;
                case L'M': case L'm': size *= (1024ULL * 1024); break;
                case L'G': case L'g': size *= (1024ULL * 1024 * 1024); break;
                case L'T': case L't': size *= (1024ULL * 1024 * 1024 * 1024); break;
                default: break;
            }
        }
        return size > 0 ? size : 1024;
    } catch (...) {
        return 1024;
    }
}
