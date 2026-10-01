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
 * SINGLE FILE INDEX: base.cpp
 * ============================================================================
 * WinBase - Object-Oriented Base64 and Base32 Encoder / Decoder for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & CONFIGURATION] ............. BaseOptions class (CLI parsing & flags)
 * 2. [STRUCTURED OUTPUT REPORTER] .......... BaseReporter class (JSON/CSV/Table/Pipe)
 * 3. [ENCODING & DECODING CODECS] .......... Base64Codec and Base32Codec classes
 * 4. [STREAM TRANSLATOR ENGINE] ............ BaseEngine class (stream reading, wrap, output)
 * 5. [APPLICATION CONTROLLER] .............. BaseApp class and main entry point
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <array>
#include <optional>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <memory>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

// ============================================================================
// 1. OPTIONS & CONFIGURATION
// ============================================================================

enum class BaseAlgorithm { Base64, Base32 };

class BaseOptions {
public:
    static constexpr std::string_view APP_NAME = "base";
    static constexpr std::string_view APP_VERSION = "8.0.0";
    static constexpr size_t BUFFER_SIZE = 65536;

    BaseAlgorithm algorithm{BaseAlgorithm::Base64};
    bool decode{false};
    bool ignoreGarbage{false};
    size_t wrapCols{76}; // 0 disables wrapping
    std::optional<std::string> inputPath;
    std::optional<std::string> outputPath;
    int outputFormat{0};
    std::string pipeCommand;

    static void printHelp() {
        std::cout << R"(base(1)                 CrossShell for UNIX Reference Manual                  base(1)

    NAME
        base - encode and decode Base64/Base32 data streams

    SYNOPSIS
        base [OPTIONS] [FILE]

    DESCRIPTION
        base encodes or decodes FILE, or standard input, to standard output.
        It supports both Base64 and Base32 transformations and can also emit
        structured output formats for pipelines and scripting use.

    OPTIONS
        --base64
            Use Base64 encoding/decoding (default).

        --base32
            Use Base32 encoding/decoding.

        -d, --decode
            Decode data instead of encoding it.

        -i, --ignore-garbage
            When decoding, ignore non-alphabet characters.

        -w, --wrap COLS
            Wrap encoded lines after COLS characters (default 76, 0 disables
            wrapping).

        -o, --output FILE
            Write output to FILE instead of standard output.

        --json, --csv
            Output structured records.

        --pipe COMMAND
            Send output through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        base --base64 sample.txt
            Encode file using Base64.

        base -d --output decoded.bin encoded.txt
            Decode Base64 file into binary output.

        base --json --pipe jq
            Output structured JSON and pipe to jq.

    CrossShell for UNIX                                                    base(1)
)";
    }

    static void printVersion() {
        std::cout << APP_NAME << " " << APP_VERSION << "\n";
    }

    static bool parse(int argc, char* argv[], BaseOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help" || arg == "-h" || arg == "/?") {
                printHelp();
                std::exit(0);
            } else if (arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "--base64") {
                opts.algorithm = BaseAlgorithm::Base64;
            } else if (arg == "--base32") {
                opts.algorithm = BaseAlgorithm::Base32;
            } else if (arg == "-d" || arg == "--decode") {
                opts.decode = true;
            } else if (arg == "-i" || arg == "--ignore-garbage") {
                opts.ignoreGarbage = true;
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg.rfind("-w", 0) == 0) {
                opts.wrapCols = std::stoul(arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "76"));
            } else if (arg.rfind("--wrap=", 0) == 0) {
                opts.wrapCols = std::stoul(arg.substr(7));
            } else if (arg.rfind("-o", 0) == 0) {
                opts.outputPath = arg.size() > 2 ? arg.substr(2) : (i + 1 < argc ? argv[++i] : "");
            } else if (arg.rfind("--output=", 0) == 0) {
                opts.outputPath = arg.substr(9);
            } else if (arg[0] == '-' && arg.size() > 1) {
                std::cerr << "base: unrecognized option '" << arg << "'\n";
                return false;
            } else {
                opts.inputPath = arg;
            }
        }
        return true;
    }
};

// ============================================================================
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class BaseReporter {
public:
    static int dispatch(const std::string& content, int format, const std::string& pipeCommand, std::ostream& directOut) {
        if (format != 0 || !pipeCommand.empty()) {
            std::string text;
            if (format == 1) {
                text = "{\"data\":\"" + content + "\"}\n";
            } else if (format == 2) {
                text = "data\n\"" + content + "\"\n";
            } else if (format == 3) {
                text = "DATA\n----\n" + content;
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
// 3. ENCODING & DECODING CODECS
// ============================================================================

class Base64Codec {
private:
    static inline const char ENCODE_TABLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

public:
    static std::string encode(const std::vector<uint8_t>& data) {
        std::string result;
        result.reserve(((data.size() + 2) / 3) * 4);

        size_t i = 0;
        while (i < data.size()) {
            uint32_t octet_a = (i < data.size()) ? data[i++] : 0;
            uint32_t octet_b = (i < data.size()) ? data[i++] : 0;
            uint32_t octet_c = (i < data.size()) ? data[i++] : 0;

            uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

            result.push_back(ENCODE_TABLE[(triple >> 18) & 0x3F]);
            result.push_back(ENCODE_TABLE[(triple >> 12) & 0x3F]);
            result.push_back((i > data.size() + 1) ? '=' : ENCODE_TABLE[(triple >> 6) & 0x3F]);
            result.push_back((i > data.size()) ? '=' : ENCODE_TABLE[triple & 0x3F]);
        }
        return result;
    }

    static std::vector<uint8_t> decode(const std::string& input, bool ignoreGarbage) {
        std::vector<uint8_t> result;
        std::vector<int> table(256, -1);
        for (int i = 0; i < 64; ++i) table[static_cast<unsigned char>(ENCODE_TABLE[i])] = i;

        uint32_t val = 0;
        int valb = -8;
        for (unsigned char c : input) {
            if (c == '=') break;
            int d = table[c];
            if (d == -1) {
                if (ignoreGarbage || std::isspace(c)) continue;
                throw std::runtime_error("invalid character in input");
            }
            val = (val << 6) | d;
            valb += 6;
            if (valb >= 0) {
                result.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return result;
    }
};

class Base32Codec {
private:
    static inline const char ENCODE_TABLE[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

public:
    static std::string encode(const std::vector<uint8_t>& data) {
        std::string result;
        result.reserve(((data.size() + 4) / 5) * 8);

        size_t i = 0;
        while (i < data.size()) {
            uint64_t buffer = 0;
            int bytes = 0;
            for (int k = 0; k < 5; ++k) {
                buffer <<= 8;
                if (i < data.size()) {
                    buffer |= data[i++];
                    bytes++;
                }
            }

            int pad = 0;
            if (bytes == 1) pad = 6;
            else if (bytes == 2) pad = 4;
            else if (bytes == 3) pad = 3;
            else if (bytes == 4) pad = 1;

            for (int bit = 35; bit >= 0; bit -= 5) {
                result.push_back(ENCODE_TABLE[(buffer >> bit) & 0x1F]);
            }

            for (int p = 0; p < pad; ++p) {
                result[result.size() - 1 - p] = '=';
            }
        }
        return result;
    }

    static std::vector<uint8_t> decode(const std::string& input, bool ignoreGarbage) {
        std::vector<uint8_t> result;
        std::vector<int> table(256, -1);
        for (int i = 0; i < 32; ++i) table[static_cast<unsigned char>(ENCODE_TABLE[i])] = i;

        uint64_t val = 0;
        int valb = -8;
        for (unsigned char c : input) {
            if (c == '=') break;
            int d = table[c];
            if (d == -1) {
                if (ignoreGarbage || std::isspace(c)) continue;
                throw std::runtime_error("invalid character in input");
            }
            val = (val << 5) | d;
            valb += 5;
            if (valb >= 0) {
                result.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return result;
    }
};

// ============================================================================
// 4. STREAM TRANSLATOR ENGINE
// ============================================================================

class BaseEngine {
private:
    BaseOptions options;

public:
    explicit BaseEngine(BaseOptions opts) : options(std::move(opts)) {}

    int execute() {
#ifdef _WIN32
        _setmode(_fileno(stdin), _O_BINARY);
        _setmode(_fileno(stdout), _O_BINARY);
#endif

        std::istream* in = &std::cin;
        std::ifstream fileIn;

        if (options.inputPath && *options.inputPath != "-") {
            fileIn.open(*options.inputPath, std::ios::binary);
            if (!fileIn.is_open()) {
                std::cerr << "base: cannot open '" << *options.inputPath << "'\n";
                return 1;
            }
            in = &fileIn;
        }

        std::ostream* out = &std::cout;
        std::ofstream fileOut;

        if (options.outputPath) {
            fileOut.open(*options.outputPath, std::ios::binary);
            if (!fileOut.is_open()) {
                std::cerr << "base: cannot open '" << *options.outputPath << "' for writing\n";
                return 1;
            }
            out = &fileOut;
        }

        std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(*in)), std::istreambuf_iterator<char>());

        if (options.decode) {
            try {
                std::string inputStr(buffer.begin(), buffer.end());
                std::vector<uint8_t> decoded = (options.algorithm == BaseAlgorithm::Base64)
                    ? Base64Codec::decode(inputStr, options.ignoreGarbage)
                    : Base32Codec::decode(inputStr, options.ignoreGarbage);

                std::string text(decoded.begin(), decoded.end());
                BaseReporter::dispatch(text, options.outputFormat, options.pipeCommand, *out);
            } catch (const std::exception& e) {
                std::cerr << "base: error decoding input: " << e.what() << "\n";
                return 1;
            }
        } else {
            std::string encoded = (options.algorithm == BaseAlgorithm::Base64)
                ? Base64Codec::encode(buffer)
                : Base32Codec::encode(buffer);

            if (options.wrapCols > 0) {
                std::string wrapped;
                for (size_t i = 0; i < encoded.size(); i += options.wrapCols) {
                    wrapped += encoded.substr(i, options.wrapCols) + "\n";
                }
                encoded = wrapped;
            } else {
                encoded += "\n";
            }

            BaseReporter::dispatch(encoded, options.outputFormat, options.pipeCommand, *out);
        }

        return 0;
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class BaseApp {
public:
    static int run(int argc, char* argv[]) {
        BaseOptions options;
        if (!BaseOptions::parse(argc, argv, options)) {
            return 1;
        }
        BaseEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return BaseApp::run(argc, argv);
}