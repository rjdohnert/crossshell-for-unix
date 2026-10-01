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
 * SINGLE FILE INDEX: md5sum.cpp
 * ============================================================================
 * WinMd5sum - Object-Oriented MD5 Message-Digest & Verification Utility for Windows
 * Specification: RFC 1321 | C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [MD5 CORE DIGEST ALGORITHM] ........... MD5DigestEngine class (RFC 1321)
 * 2. [OPTIONS & CONFIGURATION] ............. Md5sumOptions class (CLI parsing & flags)
 * 3. [STRUCTURED OUTPUT REPORTER] .......... Md5sumReporter class (JSON/CSV/Table/Pipe)
 * 4. [CHECKSUM VERIFICATION & HASH ENGINE] . ChecksumVerifier, Md5sumEngine classes
 * 5. [APPLICATION CONTROLLER] .............. Md5sumApp class and main entry point
 * ============================================================================
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <algorithm>
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. MD5 CORE DIGEST ALGORITHM (RFC 1321)
// ============================================================================

class MD5DigestEngine {
private:
    uint32_t state[4];
    uint32_t count[2];
    uint8_t buffer[64];

    static inline uint32_t F(uint32_t x, uint32_t y, uint32_t z) { return (x & y) | (~x & z); }
    static inline uint32_t G(uint32_t x, uint32_t y, uint32_t z) { return (x & z) | (y & ~z); }
    static inline uint32_t H(uint32_t x, uint32_t y, uint32_t z) { return x ^ y ^ z; }
    static inline uint32_t I(uint32_t x, uint32_t y, uint32_t z) { return y ^ (x | ~z); }

    static inline uint32_t rotateLeft(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

    static inline void FF(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotateLeft(a + F(b, c, d) + x + ac, s) + b;
    }
    static inline void GG(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotateLeft(a + G(b, c, d) + x + ac, s) + b;
    }
    static inline void HH(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotateLeft(a + H(b, c, d) + x + ac, s) + b;
    }
    static inline void II(uint32_t& a, uint32_t b, uint32_t c, uint32_t d, uint32_t x, uint32_t s, uint32_t ac) {
        a = rotateLeft(a + I(b, c, d) + x + ac, s) + b;
    }

    void decode(uint32_t* output, const uint8_t* input, size_t len) {
        for (size_t i = 0, j = 0; j < len; i++, j += 4) {
            output[i] = ((uint32_t)input[j]) | (((uint32_t)input[j+1]) << 8) |
                        (((uint32_t)input[j+2]) << 16) | (((uint32_t)input[j+3]) << 24);
        }
    }

    void encode(uint8_t* output, const uint32_t* input, size_t len) {
        for (size_t i = 0, j = 0; j < len; i++, j += 4) {
            output[j] = (uint8_t)(input[i] & 0xff);
            output[j+1] = (uint8_t)((input[i] >> 8) & 0xff);
            output[j+2] = (uint8_t)((input[i] >> 16) & 0xff);
            output[j+3] = (uint8_t)((input[i] >> 24) & 0xff);
        }
    }

    void transform(const uint8_t block[64]) {
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3], x[16];
        decode(x, block, 64);

        /* Round 1 */
        FF(a, b, c, d, x[ 0], 7, 0xd76aa478);
        FF(d, a, b, c, x[ 1], 12, 0xe8c7b756);
        FF(c, d, a, b, x[ 2], 17, 0x242070db);
        FF(b, c, d, a, x[ 3], 22, 0xc1bdceee);
        FF(a, b, c, d, x[ 4], 7, 0xf57c0faf);
        FF(d, a, b, c, x[ 5], 12, 0x4787c62a);
        FF(c, d, a, b, x[ 6], 17, 0xa8304613);
        FF(b, c, d, a, x[ 7], 22, 0xfd469501);
        FF(a, b, c, d, x[ 8], 7, 0x698098d8);
        FF(d, a, b, c, x[ 9], 12, 0x8b44f7af);
        FF(c, d, a, b, x[10], 17, 0xffff5bb1);
        FF(b, c, d, a, x[11], 22, 0x895cd7be);
        FF(a, b, c, d, x[12], 7, 0x6b901122);
        FF(d, a, b, c, x[13], 12, 0xfd987193);
        FF(c, d, a, b, x[14], 17, 0xa679438e);
        FF(b, c, d, a, x[15], 22, 0x49b40821);

        /* Round 2 */
        GG(a, b, c, d, x[ 1], 5, 0xf61e2562);
        GG(d, a, b, c, x[ 6], 9, 0xc040b340);
        GG(c, d, a, b, x[11], 14, 0x265e5a51);
        GG(b, c, d, a, x[ 0], 20, 0xe9b6c7aa);
        GG(a, b, c, d, x[ 5], 5, 0xd62f105d);
        GG(d, a, b, c, x[10], 9, 0x02441453);
        GG(c, d, a, b, x[15], 14, 0xd8a1e681);
        GG(b, c, d, a, x[ 4], 20, 0xe7d3fbc8);
        GG(a, b, c, d, x[ 9], 5, 0x21e1cde6);
        GG(d, a, b, c, x[14], 9, 0xc33707d6);
        GG(c, d, a, b, x[ 3], 14, 0xf4d50d87);
        GG(b, c, d, a, x[ 8], 20, 0x455a14ed);
        GG(a, b, c, d, x[13], 5, 0xa9e3e905);
        GG(d, a, b, c, x[ 2], 9, 0xfcefa3f8);
        GG(c, d, a, b, x[ 7], 14, 0x676f02d9);
        GG(b, c, d, a, x[12], 20, 0x8d2a4c8a);

        /* Round 3 */
        HH(a, b, c, d, x[ 5], 4, 0xfffa3942);
        HH(d, a, b, c, x[ 8], 11, 0x8771f681);
        HH(c, d, a, b, x[11], 16, 0x6d9d6122);
        HH(b, c, d, a, x[14], 23, 0xfde5380c);
        HH(a, b, c, d, x[ 1], 4, 0xa4beea44);
        HH(d, a, b, c, x[ 4], 11, 0x4bdecfa9);
        HH(c, d, a, b, x[ 7], 16, 0xf6bb4b60);
        HH(b, c, d, a, x[10], 23, 0xbebfbc70);
        HH(a, b, c, d, x[13], 4, 0x289b7ec6);
        HH(d, a, b, c, x[ 0], 11, 0xeaa127fa);
        HH(c, d, a, b, x[ 3], 16, 0xd4ef3085);
        HH(b, c, d, a, x[ 6], 23, 0x04881d05);
        HH(a, b, c, d, x[ 9], 4, 0xd9d4d039);
        HH(d, a, b, c, x[12], 11, 0xe6db99e5);
        HH(c, d, a, b, x[15], 16, 0x1fa27cf8);
        HH(b, c, d, a, x[14], 23, 0xc4ac5665);

        /* Round 4 */
        II(a, b, c, d, x[ 0], 6, 0xf4292244);
        II(d, a, b, c, x[ 7], 10, 0x432aff97);
        II(c, d, a, b, x[14], 15, 0xab9423a7);
        II(b, c, d, a, x[ 5], 21, 0xfc93a039);
        II(a, b, c, d, x[12], 6, 0x655b59c3);
        II(d, a, b, c, x[ 3], 10, 0x8f0ccc92);
        II(c, d, a, b, x[10], 15, 0xffeff47d);
        II(b, c, d, a, x[ 1], 21, 0x85845dd1);
        II(a, b, c, d, x[ 6], 6, 0x6fa87e4f);
        II(d, a, b, c, x[11], 10, 0xfe2ce6e0);
        II(c, d, a, b, x[ 4], 15, 0xa3014314);
        II(b, c, d, a, x[ 9], 21, 0x4e0811a1);
        II(a, b, c, d, x[14], 6, 0xf7537e82);
        II(d, a, b, c, x[ 5], 10, 0xbd3af235);
        II(c, d, a, b, x[12], 15, 0x2ad7d2bb);
        II(b, c, d, a, x[ 3], 21, 0xeb86d391);

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
    }

public:
    MD5DigestEngine() { init(); }

    void init() {
        count[0] = count[1] = 0;
        state[0] = 0x67452301;
        state[1] = 0xefcdab89;
        state[2] = 0x98badcfe;
        state[3] = 0x10325476;
    }

    void update(const uint8_t* input, size_t inputLen) {
        size_t i, index, partLen;
        index = (size_t)((count[0] >> 3) & 0x3F);
        if ((count[0] += ((uint32_t)inputLen << 3)) < ((uint32_t)inputLen << 3)) {
            count[1]++;
        }
        count[1] += ((uint32_t)inputLen >> 29);

        partLen = 64 - index;

        if (inputLen >= partLen) {
            std::memcpy(&buffer[index], input, partLen);
            transform(buffer);

            for (i = partLen; i + 63 < inputLen; i += 64) {
                transform(&input[i]);
            }
            index = 0;
        } else {
            i = 0;
        }

        std::memcpy(&buffer[index], &input[i], inputLen - i);
    }

    std::string finalize() {
        uint8_t bits[8];
        encode(bits, count, 8);

        size_t index = (size_t)((count[0] >> 3) & 0x3f);
        size_t padLen = (index < 56) ? (56 - index) : (120 - index);

        static const uint8_t PADDING[64] = { 0x80 };
        update(PADDING, padLen);
        update(bits, 8);

        uint8_t digest[16];
        encode(digest, state, 16);

        std::ostringstream ss;
        for (int i = 0; i < 16; ++i) {
            ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
        }
        return ss.str();
    }
};

// ============================================================================
// 2. OPTIONS & CONFIGURATION
// ============================================================================

class Md5sumOptions {
public:
    bool binaryMode{true};
    bool doCheck{false};
    bool quiet{false};
    bool status{false};
    bool warn{false};
    std::vector<std::string> files;
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* progName = nullptr) {
        std::cout << R"(md5sum(1)               CrossShell for UNIX Reference Manual               md5sum(1)

    NAME
        md5sum - compute and check MD5 message digest

    SYNOPSIS
        md5sum [OPTIONS]... [FILE]...

    DESCRIPTION
        Print or check MD5 (128-bit) checksums.
        With no FILE, or when FILE is -, read standard input.

    OPTIONS
        -b, --binary
            Read in binary mode (default on Windows).

        -t, --text
            Read in text mode.

        -c, --check
            Read MD5 sums from the FILEs and check them.

        --status
            Don't output anything, status code shows success.

        -q, --quiet
            Don't print OK for each successfully verified file.

        -w, --warn
            Warn about improperly formatted checksum lines.

        --json
            Output results in JSON format.

        --csv
            Output results in CSV format.

        --table
            Output results in formatted ASCII table.

        --pipe <command>
            Send output through COMMAND pipeline.

        -h, --help
            Display this reference manual and exit.

        -V, --version
            Display version information and exit.

    EXAMPLES
        md5sum file.iso
            Compute the MD5 checksum of file.iso.

        md5sum -c MD5SUMS
            Verify all checksums listed in MD5SUMS.

        md5sum --json *.dll
            Generate structured JSON checksum records for DLL files.

    EXIT STATUS
        0
            Success.

        1
            An error occurred, or at least one checksum did not match.

    CrossShell for UNIX                                                   md5sum(1)
)";
    }

    static void printVersion() {
        std::cout << "md5sum 1.0.0\n";
    }

    static bool parse(int argc, char* argv[], Md5sumOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "-V" || arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-b" || arg == "--binary") {
                opts.binaryMode = true;
            } else if (arg == "-t" || arg == "--text") {
                opts.binaryMode = false;
            } else if (arg == "-c" || arg == "--check") {
                opts.doCheck = true;
            } else if (arg == "--status") {
                opts.status = true;
            } else if (arg == "-q" || arg == "--quiet") {
                opts.quiet = true;
            } else if (arg == "-w" || arg == "--warn") {
                opts.warn = true;
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (!arg.empty() && arg[0] == '-' && arg != "-") {
                std::cerr << "md5sum: invalid option '" << arg << "'\n";
                return false;
            } else {
                opts.files.push_back(arg);
            }
        }

        if (opts.files.empty()) {
            opts.files.push_back("-");
        }

        return true;
    }
};

// ============================================================================
// 3. STRUCTURED OUTPUT REPORTER
// ============================================================================

struct HashResult {
    std::string filename;
    std::string hash;
    bool isBinary{true};
    bool error{false};
};

class Md5sumReporter {
public:
    static int dispatch(const std::vector<HashResult>& results, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"md5\":[";
            for (size_t i = 0; i < results.size(); ++i) {
                if (i > 0) text += ",";
                text += "{\"file\":\"" + results[i].filename + "\",\"hash\":\"" + results[i].hash + "\"}";
            }
            text += "]}\n";
        } else if (format == 2) {
            text = "hash,file\n";
            for (const auto& r : results) {
                text += "\"" + r.hash + "\",\"" + r.filename + "\"\n";
            }
        } else if (format == 3) {
            text = "HASH                              FILE\n--------------------------------  ----\n";
            for (const auto& r : results) {
                text += r.hash + "  " + r.filename + "\n";
            }
        } else {
            for (const auto& r : results) {
                if (!r.error) {
                    text += r.hash + " " + (r.isBinary ? "*" : " ") + r.filename + "\n";
                }
            }
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
// 4. CHECKSUM VERIFICATION & HASH ENGINE
// ============================================================================

class ChecksumVerifier {
public:
    static bool parseLine(const std::string& line, std::string& expectedHash, bool& isBinary, std::string& filename) {
        if (line.length() < 34) return false;

        expectedHash = line.substr(0, 32);
        for (char c : expectedHash) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
        }

        size_t idx = 32;
        if (line[idx] != ' ' && line[idx] != '\t') return false;

        idx++;
        if (idx >= line.length()) return false;

        if (line[idx] == '*') {
            isBinary = true;
            idx++;
        } else if (line[idx] == ' ') {
            isBinary = false;
            idx++;
        } else {
            isBinary = false;
        }

        if (idx >= line.length()) return false;
        filename = line.substr(idx);

        if (!filename.empty() && filename.back() == '\r') {
            filename.pop_back();
        }

        return true;
    }
};

class Md5sumEngine {
private:
    Md5sumOptions options;

    static std::string computeStream(std::istream& is) {
        MD5DigestEngine md5;
        char buffer[65536];
        while (is.read(buffer, sizeof(buffer)) || is.gcount() > 0) {
            md5.update(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(is.gcount()));
        }
        return md5.finalize();
    }

public:
    explicit Md5sumEngine(Md5sumOptions opts) : options(std::move(opts)) {}

    static std::string computeFile(const std::string& filepath, bool binaryMode, bool& error) {
        error = false;
        if (filepath == "-") {
            _setmode(_fileno(stdin), binaryMode ? _O_BINARY : _O_TEXT);
            return computeStream(std::cin);
        }

        std::ios_base::openmode mode = std::ios::in;
        if (binaryMode) mode |= std::ios::binary;

        std::ifstream file(filepath, mode);
        if (!file.is_open()) {
            error = true;
            return "";
        }

        return computeStream(file);
    }

    int executeCheck() {
        size_t readFailures = 0;
        size_t checksumMismatches = 0;
        size_t formatErrors = 0;
        size_t totalChecked = 0;

        for (const auto& file : options.files) {
            std::istream* inStream = &std::cin;
            std::ifstream infile;

            if (file != "-") {
                infile.open(file);
                if (!infile.is_open()) {
                    if (!options.status) {
                        std::cerr << "md5sum: " << file << ": No such file or directory\n";
                    }
                    readFailures++;
                    continue;
                }
                inStream = &infile;
            }

            std::string line;
            size_t lineNum = 0;

            while (std::getline(*inStream, line)) {
                lineNum++;
                if (line.empty() || line[0] == '#') continue;

                std::string expectedHash, filename;
                bool lineIsBinary = options.binaryMode;

                if (!ChecksumVerifier::parseLine(line, expectedHash, lineIsBinary, filename)) {
                    formatErrors++;
                    if (options.warn && !options.status) {
                        std::cerr << "md5sum: " << file << ": " << lineNum << ": improperly formatted MD5 checksum line\n";
                    }
                    continue;
                }

                bool err = false;
                std::string actualHash = computeFile(filename, lineIsBinary, err);
                totalChecked++;

                if (err) {
                    readFailures++;
                    if (!options.status) {
                        std::cout << filename << ": FAILED open or read\n";
                    }
                } else {
                    std::string expLower = expectedHash;
                    std::transform(expLower.begin(), expLower.end(), expLower.begin(), ::tolower);

                    if (actualHash == expLower) {
                        if (!options.status && !options.quiet) {
                            std::cout << filename << ": OK\n";
                        }
                    } else {
                        checksumMismatches++;
                        if (!options.status) {
                            std::cout << filename << ": FAILED\n";
                        }
                    }
                }
            }
        }

        if (!options.status) {
            if (formatErrors > 0 && !options.warn) {
                std::cerr << "md5sum: WARNING: " << formatErrors << " line(s) improperly formatted\n";
            }
            if (readFailures > 0) {
                std::cerr << "md5sum: WARNING: " << readFailures << " listed file(s) could not be read\n";
            }
            if (checksumMismatches > 0) {
                std::cerr << "md5sum: WARNING: " << checksumMismatches << " computed checksum(s) did NOT match\n";
            }
        }

        return (checksumMismatches > 0 || readFailures > 0 || totalChecked == 0) ? 1 : 0;
    }

    int executeCompute() {
        int exitCode = 0;
        std::vector<HashResult> results;

        for (const auto& file : options.files) {
            bool err = false;
            std::string hash = computeFile(file, options.binaryMode, err);

            if (err) {
                std::cerr << "md5sum: " << file << ": No such file or directory\n";
                exitCode = 1;
                results.push_back({ file, "", options.binaryMode, true });
            } else {
                results.push_back({ file, hash, options.binaryMode, false });
            }
        }

        Md5sumReporter::dispatch(results, options.outputFormat, options.pipeCommand);
        return exitCode;
    }

    int execute() {
        if (options.doCheck) {
            return executeCheck();
        }
        return executeCompute();
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class Md5sumApp {
public:
    static int run(int argc, char* argv[]) {
        Md5sumOptions options;
        if (!Md5sumOptions::parse(argc, argv, options)) {
            return 1;
        }
        Md5sumEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return Md5sumApp::run(argc, argv);
}
