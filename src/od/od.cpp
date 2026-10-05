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
 * SINGLE FILE INDEX: od.cpp
 * ============================================================================
 * WinOd - Object-Oriented Octal / Hex / Decimal Data Dumper for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & FORMAT STRUCTURES] ......... FormatSpec and OdOptions classes
 * 2. [STRUCTURED OUTPUT REPORTER] .......... OdReporter class (JSON/CSV/Table/Pipe)
 * 3. [DATA DUMP & BYTE FORMATTER ENGINE] ... OdFormatter and OdEngine classes
 * 4. [APPLICATION CONTROLLER] .............. OdApp class and main entry point
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. OPTIONS & FORMAT STRUCTURES
// ============================================================================

enum class FormatKind {
    ASCII_NAMED,  // -a
    CHAR_ESCAPE,  // -c
    SIGNED_DEC,   // -d / -t d
    UNSIGNED_DEC, // -u / -t u
    OCTAL,        // -o / -t o
    HEX,          // -x / -t x
    FLOAT         // -f / -t f
};

struct FormatSpec {
    FormatKind kind;
    size_t size; // Byte size: 1, 2, 4, 8
};

class OdOptions {
public:
    char addressRadix{'o'}; // 'o', 'd', 'x', 'n'
    uint64_t skipBytes{0};
    uint64_t limitBytes{UINT64_MAX};
    bool outputDuplicates{false}; // -v
    size_t bytesPerLine{16};      // -w
    std::vector<FormatSpec> formats;
    std::vector<std::string> filenames;
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* prog) {
        std::cout << "Usage: " << prog << " [OPTION]... [FILE]...\n"
                  << "Write an unambiguous representation, octal bytes by default,\n"
                  << "of FILE to standard output. With no FILE, read standard input.\n\n"
                  << "Options:\n"
                  << "  -A, --address-radix=RADIX  decide how file offsets are printed (d, o, x, n)\n"
                  << "  -j, --skip-bytes=BYTES     skip BYTES input bytes before formatting\n"
                  << "  -N, --read-bytes=BYTES     limit dump to BYTES input bytes\n"
                  << "  -v, --output-duplicates    do not use * to mark line suppression\n"
                  << "  -w, --width[=BYTES]        format BYTES bytes per output line (default 16)\n"
                  << "  -a                         same as -t a, select named characters\n"
                  << "  -b                         same as -t o1, select octal bytes\n"
                  << "  -c                         same as -t c, select printable chars or escapes\n"
                  << "  -d                         same as -t u2, select unsigned decimal 2-byte units\n"
                  << "  -o                         same as -t o2, select octal 2-byte units\n"
                  << "  -x                         same as -t x2, select hexadecimal 2-byte units\n"
                  << "  -t, --format=TYPE          select output format or type\n"
                  << "      --json, --csv, --table structured output formats\n"
                  << "      --pipe COMMAND         send formatted output through COMMAND\n"
                  << "  -h, --help                 display this help and exit\n"
                  << "      --version              output version information and exit\n";
    }

    static void printVersion() {
        std::cout << "od 1.0.0\n";
    }

    static uint64_t parseByteCount(const std::string& str) {
        size_t idx = 0;
        uint64_t val = std::stoull(str, &idx);
        if (idx < str.length()) {
            char suffix = str[idx];
            if (suffix == 'k' || suffix == 'K') val *= 1024;
            else if (suffix == 'm' || suffix == 'M') val *= 1024 * 1024;
            else if (suffix == 'g' || suffix == 'G') val *= 1024 * 1024 * 1024;
            else if (suffix == 'b') val *= 512;
        }
        return val;
    }

    static bool parseTypeString(const std::string& typeStr, std::vector<FormatSpec>& formats) {
        if (typeStr.empty()) return false;
        size_t i = 0;
        while (i < typeStr.length()) {
            char code = typeStr[i++];
            size_t size = 4;

            if (code == 'a') {
                formats.push_back({ FormatKind::ASCII_NAMED, 1 });
                continue;
            } else if (code == 'c') {
                formats.push_back({ FormatKind::CHAR_ESCAPE, 1 });
                continue;
            }

            if (code == 'f') size = sizeof(double);
            else if (code == 'd' || code == 'u' || code == 'o' || code == 'x') size = sizeof(int);
            else return false;

            if (i < typeStr.length()) {
                char s = typeStr[i];
                if (s == 'C' || s == '1') { size = 1; i++; }
                else if (s == 'S' || s == '2') { size = 2; i++; }
                else if (s == 'I' || s == '4') { size = 4; i++; }
                else if (s == 'L' || s == '8') { size = 8; i++; }
                else if (s == 'F') { size = sizeof(float); i++; }
                else if (s == 'D') { size = sizeof(double); i++; }
            }

            switch (code) {
                case 'd': formats.push_back({ FormatKind::SIGNED_DEC, size }); break;
                case 'u': formats.push_back({ FormatKind::UNSIGNED_DEC, size }); break;
                case 'o': formats.push_back({ FormatKind::OCTAL, size }); break;
                case 'x': formats.push_back({ FormatKind::HEX, size }); break;
                case 'f': formats.push_back({ FormatKind::FLOAT, size }); break;
            }
        }
        return true;
    }

    static bool parse(int argc, char* argv[], OdOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-v" || arg == "--output-duplicates") {
                opts.outputDuplicates = true;
            } else if (arg == "-a") {
                opts.formats.push_back({ FormatKind::ASCII_NAMED, 1 });
            } else if (arg == "-b") {
                opts.formats.push_back({ FormatKind::OCTAL, 1 });
            } else if (arg == "-c") {
                opts.formats.push_back({ FormatKind::CHAR_ESCAPE, 1 });
            } else if (arg == "-d") {
                opts.formats.push_back({ FormatKind::UNSIGNED_DEC, 2 });
            } else if (arg == "-o") {
                opts.formats.push_back({ FormatKind::OCTAL, 2 });
            } else if (arg == "-x") {
                opts.formats.push_back({ FormatKind::HEX, 2 });
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg.rfind("-A", 0) == 0) {
                std::string r = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "o");
                if (!r.empty()) opts.addressRadix = r[0];
            } else if (arg.rfind("-j", 0) == 0) {
                std::string s = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "0");
                opts.skipBytes = parseByteCount(s);
            } else if (arg.rfind("-N", 0) == 0) {
                std::string s = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "0");
                opts.limitBytes = parseByteCount(s);
            } else if (arg.rfind("-w", 0) == 0) {
                std::string s = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "16");
                opts.bytesPerLine = static_cast<size_t>(parseByteCount(s));
            } else if (arg.rfind("-t", 0) == 0) {
                std::string t = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "o2");
                parseTypeString(t, opts.formats);
            } else if (arg[0] == '-' && arg.size() > 1) {
                std::cerr << "od: unrecognized option '" << arg << "'\n";
                return false;
            } else {
                opts.filenames.push_back(arg);
            }
        }

        if (opts.formats.empty()) {
            opts.formats.push_back({ FormatKind::OCTAL, 2 });
        }

        if (opts.filenames.empty()) {
            opts.filenames.push_back("-");
        }

        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class OdReporter {
public:
    static int dispatch(const std::string& content, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"od\":\"" + content + "\"}\n";
        } else if (format == 2) {
            text = "od\n\"" + content + "\"\n";
        } else if (format == 3) {
            text = "OD\n--\n" + content + "\n";
        } else {
            text = content;
        }

        if (!pipeCommand.empty()) {
            FILE* pipe = _popen(pipeCommand.c_str(), "w");
            if (!pipe) return 1;
            std::fwrite(text.data(), 1, text.size(), pipe);
            _pclose(pipe);
        } else {
            std::cout << text;
        }
        return 0;
    }
};

// ============================================================================
// 3. DATA DUMP & BYTE FORMATTER ENGINE
// ============================================================================

class OdFormatter {
private:
    static inline const char* ASCII_NAMES[128] = {
        "nul", "soh", "stx", "etx", "eot", "enq", "ack", "bel",
        "bs",  "ht",  "nl",  "vt",  "ff",  "cr",  "so",  "si",
        "dle", "dc1", "dc2", "dc3", "dc4", "nak", "syn", "etb",
        "can", "em",  "sub", "esc", "fs",  "gs",  "rs",  "us",
        "sp",  "!",   "\"",  "#",   "$",   "%",   "&",   "'",
        "(",   ")",   "*",   "+",   ",",   "-",   ".",   "/",
        "0",   "1",   "2",   "3",   "4",   "5",   "6",   "7",
        "8",   "9",   ":",   ";",   "<",   "=",   ">",   "?",
        "@",   "A",   "B",   "C",   "D",   "E",   "F",   "G",
        "H",   "I",   "J",   "K",   "L",   "M",   "N",   "O",
        "P",   "Q",   "R",   "S",   "T",   "U",   "V",   "W",
        "X",   "Y",   "Z",   "[",   "\\",  "]",   "^",   "_",
        "`",   "a",   "b",   "c",   "d",   "e",   "f",   "g",
        "h",   "i",   "j",   "k",   "l",   "m",   "n",   "o",
        "p",   "q",   "r",   "s",   "t",   "u",   "v",   "w",
        "x",   "y",   "z",   "{",   "|",   "}",   "~",   "del"
    };

public:
    static void printAddress(std::ostream& out, uint64_t addr, char radix) {
        if (radix == 'n') return;
        std::ios state(nullptr);
        state.copyfmt(out);

        if (radix == 'o') {
            out << std::setfill('0') << std::setw(7) << std::oct << addr << " ";
        } else if (radix == 'x') {
            out << std::setfill('0') << std::setw(6) << std::hex << addr << " ";
        } else if (radix == 'd') {
            out << std::setfill('0') << std::setw(7) << std::dec << addr << " ";
        }
        out.copyfmt(state);
    }

    static void formatChunk(std::ostream& out, const uint8_t* data, size_t size, const FormatSpec& fmt) {
        std::ios state(nullptr);
        state.copyfmt(out);

        for (size_t i = 0; i < size; i += fmt.size) {
            size_t available = (i + fmt.size <= size) ? fmt.size : (size - i);

            if (fmt.kind == FormatKind::ASCII_NAMED) {
                uint8_t byte = data[i] & 0x7F;
                out << std::setw(3) << ASCII_NAMES[byte] << " ";
            } else if (fmt.kind == FormatKind::CHAR_ESCAPE) {
                char c = static_cast<char>(data[i]);
                if (c == '\0') out << "  \\0";
                else if (c == '\a') out << "  \\a";
                else if (c == '\b') out << "  \\b";
                else if (c == '\f') out << "  \\f";
                else if (c == '\n') out << "  \\n";
                else if (c == '\r') out << "  \\r";
                else if (c == '\t') out << "  \\t";
                else if (c == '\v') out << "  \\v";
                else if (std::isprint(static_cast<unsigned char>(c))) out << "   " << c;
                else {
                    out << " " << std::setfill('0') << std::setw(3) << std::oct << static_cast<int>(static_cast<unsigned char>(c));
                }
                out << " ";
            } else if (fmt.kind == FormatKind::HEX) {
                out << " ";
                if (fmt.size == 1) {
                    out << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(data[i]);
                } else if (fmt.size == 2) {
                    uint16_t v = 0;
                    std::memcpy(&v, data + i, available);
                    out << std::setfill('0') << std::setw(4) << std::hex << v;
                } else if (fmt.size == 4) {
                    uint32_t v = 0;
                    std::memcpy(&v, data + i, available);
                    out << std::setfill('0') << std::setw(8) << std::hex << v;
                }
            } else if (fmt.kind == FormatKind::OCTAL) {
                out << " ";
                if (fmt.size == 1) {
                    out << std::setfill('0') << std::setw(3) << std::oct << static_cast<int>(data[i]);
                } else if (fmt.size == 2) {
                    uint16_t v = 0;
                    std::memcpy(&v, data + i, available);
                    out << std::setfill('0') << std::setw(6) << std::oct << v;
                }
            } else if (fmt.kind == FormatKind::UNSIGNED_DEC) {
                out << " ";
                if (fmt.size == 2) {
                    uint16_t v = 0;
                    std::memcpy(&v, data + i, available);
                    out << std::setw(5) << std::dec << v;
                } else if (fmt.size == 4) {
                    uint32_t v = 0;
                    std::memcpy(&v, data + i, available);
                    out << std::setw(10) << std::dec << v;
                }
            } else if (fmt.kind == FormatKind::SIGNED_DEC) {
                out << " ";
                if (fmt.size == 2) {
                    int16_t v = 0;
                    std::memcpy(&v, data + i, available);
                    out << std::setw(6) << std::dec << v;
                } else if (fmt.size == 4) {
                    int32_t v = 0;
                    std::memcpy(&v, data + i, available);
                    out << std::setw(11) << std::dec << v;
                }
            }
        }
        out.copyfmt(state);
    }
};

class OdEngine {
private:
    OdOptions options;

public:
    explicit OdEngine(OdOptions opts) : options(std::move(opts)) {}

    int execute() {
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);

        std::vector<uint8_t> allBytes;

        for (const auto& fname : options.filenames) {
            if (fname == "-") {
                char buf[4096];
                while (std::cin.read(buf, sizeof(buf)) || std::cin.gcount() > 0) {
                    allBytes.insert(allBytes.end(), buf, buf + std::cin.gcount());
                }
            } else {
                std::ifstream f(fname, std::ios::binary);
                if (!f.is_open()) {
                    std::cerr << "od: " << fname << ": No such file or directory\n";
                    continue;
                }
                char buf[4096];
                while (f.read(buf, sizeof(buf)) || f.gcount() > 0) {
                    allBytes.insert(allBytes.end(), buf, buf + f.gcount());
                }
            }
        }

        std::ostringstream captured;
        std::ostream* outStream = (options.outputFormat || !options.pipeCommand.empty()) ? &captured : &std::cout;

        uint64_t start = (std::min)(options.skipBytes, static_cast<uint64_t>(allBytes.size()));
        uint64_t end = (std::min)(start + options.limitBytes, static_cast<uint64_t>(allBytes.size()));

        uint64_t currentOffset = start;
        while (currentOffset < end) {
            size_t chunkSize = static_cast<size_t>((std::min)(static_cast<uint64_t>(options.bytesPerLine), end - currentOffset));

            for (size_t fIdx = 0; fIdx < options.formats.size(); ++fIdx) {
                if (fIdx == 0) {
                    OdFormatter::printAddress(*outStream, currentOffset, options.addressRadix);
                } else if (options.addressRadix != 'n') {
                    *outStream << std::string(8, ' ');
                }
                OdFormatter::formatChunk(*outStream, allBytes.data() + currentOffset, chunkSize, options.formats[fIdx]);
                *outStream << "\n";
            }
            currentOffset += chunkSize;
        }

        if (options.addressRadix != 'n') {
            OdFormatter::printAddress(*outStream, currentOffset, options.addressRadix);
            *outStream << "\n";
        }

        if (options.outputFormat || !options.pipeCommand.empty()) {
            OdReporter::dispatch(captured.str(), options.outputFormat, options.pipeCommand);
        }

        return 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class OdApp {
public:
    static int run(int argc, char* argv[]) {
        OdOptions options;
        if (!OdOptions::parse(argc, argv, options)) {
            return 1;
        }
        OdEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return OdApp::run(argc, argv);
}
