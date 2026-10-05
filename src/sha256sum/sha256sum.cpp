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
 * SINGLE FILE INDEX: sha256sum.cpp
 * ============================================================================
 * WinSha256sum - Object-Oriented SHA-256 Hash & Verification Utility for Windows
 * Specification: FIPS 180-4 | C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [SHA-256 CORE DIGEST ALGORITHM] ....... SHA256DigestEngine class (FIPS 180-4)
 * 2. [OPTIONS & CONFIGURATION] ............. Sha256Options class (CLI parsing & flags)
 * 3. [STRUCTURED OUTPUT REPORTER] .......... Sha256Reporter class (JSON/CSV/Table/Pipe)
 * 4. [CHECKSUM VERIFICATION & HASH ENGINE] . ChecksumVerifier, Sha256Engine classes
 * 5. [APPLICATION CONTROLLER] .............. Sha256App class and main entry point
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
// 1. SHA-256 CORE DIGEST ALGORITHM (FIPS 180-4)
// ============================================================================

class SHA256DigestEngine {
private:
    uint32_t m_state[8];
    uint64_t m_count;
    uint8_t m_buffer[64];
    size_t m_buffer_len;

    static inline uint32_t rotr(uint32_t x, uint32_t n) {
        return (x >> n) | (x << (32 - n));
    }

    static inline uint32_t choose(uint32_t e, uint32_t f, uint32_t g) {
        return (e & f) ^ (~e & g);
    }

    static inline uint32_t majority(uint32_t a, uint32_t b, uint32_t c) {
        return (a & b) ^ (a & c) ^ (b & c);
    }

    static inline uint32_t sig0(uint32_t x) {
        return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22);
    }

    static inline uint32_t sig1(uint32_t x) {
        return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25);
    }

    static inline uint32_t sub0(uint32_t x) {
        return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3);
    }

    static inline uint32_t sub1(uint32_t x) {
        return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10);
    }

    void transform(const uint8_t chunk[64]) {
        static const uint32_t K[64] = {
            0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
            0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
            0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
            0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
            0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
            0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
            0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
            0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
        };

        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (static_cast<uint32_t>(chunk[i * 4]) << 24) |
                   (static_cast<uint32_t>(chunk[i * 4 + 1]) << 16) |
                   (static_cast<uint32_t>(chunk[i * 4 + 2]) << 8) |
                   (static_cast<uint32_t>(chunk[i * 4 + 3]));
        }
        for (int i = 16; i < 64; ++i) {
            w[i] = sub1(w[i - 2]) + w[i - 7] + sub0(w[i - 15]) + w[i - 16];
        }

        uint32_t a = m_state[0], b = m_state[1], c = m_state[2], d = m_state[3];
        uint32_t e = m_state[4], f = m_state[5], g = m_state[6], h = m_state[7];

        for (int i = 0; i < 64; ++i) {
            uint32_t t1 = h + sig1(e) + choose(e, f, g) + K[i] + w[i];
            uint32_t t2 = sig0(a) + majority(a, b, c);
            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        m_state[0] += a; m_state[1] += b; m_state[2] += c; m_state[3] += d;
        m_state[4] += e; m_state[5] += f; m_state[6] += g; m_state[7] += h;
    }

public:
    SHA256DigestEngine() { init(); }

    void init() {
        m_state[0] = 0x6a09e667;
        m_state[1] = 0xbb67ae85;
        m_state[2] = 0x3c6ef372;
        m_state[3] = 0xa54ff53a;
        m_state[4] = 0x510e527f;
        m_state[5] = 0x9b05688c;
        m_state[6] = 0x1f83d9ab;
        m_state[7] = 0x5be0cd19;
        m_count = 0;
        m_buffer_len = 0;
    }

    void update(const uint8_t* data, size_t len) {
        for (size_t i = 0; i < len; ++i) {
            m_buffer[m_buffer_len++] = data[i];
            if (m_buffer_len == 64) {
                transform(m_buffer);
                m_count += 512;
                m_buffer_len = 0;
            }
        }
    }

    std::string finalize() {
        uint64_t total_bits = m_count + m_buffer_len * 8;
        m_buffer[m_buffer_len++] = 0x80;

        if (m_buffer_len > 56) {
            while (m_buffer_len < 64) {
                m_buffer[m_buffer_len++] = 0x00;
            }
            transform(m_buffer);
            m_buffer_len = 0;
        }

        while (m_buffer_len < 56) {
            m_buffer[m_buffer_len++] = 0x00;
        }

        for (int i = 7; i >= 0; --i) {
            m_buffer[56 + (7 - i)] = static_cast<uint8_t>((total_bits >> (i * 8)) & 0xFF);
        }
        transform(m_buffer);

        std::ostringstream ss;
        for (int i = 0; i < 8; ++i) {
            ss << std::hex << std::setw(8) << std::setfill('0') << m_state[i];
        }
        return ss.str();
    }
};

// ============================================================================
// 2. OPTIONS & CONFIGURATION
// ============================================================================

class Sha256Options {
public:
    bool binaryMode{true};
    bool doCheck{false};
    bool quiet{false};
    bool status{false};
    bool warn{false};
    std::vector<std::string> files;
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* progName) {
        std::cout << R"(sha256sum(1)            CrossShell for UNIX Reference Manual           sha256sum(1)

    NAME
        sha256sum - compute and check SHA-256 message digests

    SYNOPSIS
        sha256sum [OPTIONS] [FILE]...

    DESCRIPTION
        Computes and verifies SHA-256 (256-bit) checksums according to FIPS 180-4.
        Reads standard input when FILE is '-' or omitted. Output can be formatted
        as plain checksum lines or structured JSON/CSV/Table records.

    OPTIONS
        -b, --binary
            Read files in binary mode (default on Windows).

        -t, --text
            Read files in text mode.

        -c, --check
            Read SHA256 sums from the FILEs and check them.

        --status
            Do not output anything; status code shows success.

        -q, --quiet
            Do not print OK for each successfully verified file.

        -w, --warn
            Warn about improperly formatted checksum lines.

        --json
            Emit checksum results in JSON format.

        --csv
            Emit checksum results in CSV format.

        --table
            Emit checksum results in tabular format.

        --pipe COMMAND
            Stream results directly into COMMAND.

        -h, --help
            Display this reference manual.

        --version
            Display version information and exit.

    EXAMPLES
        sha256sum file.iso
            Compute SHA-256 digest for file.iso.

        sha256sum -c checksums.txt
            Verify checksums listed in checksums.txt.

        sha256sum --json *.dll
            Output SHA-256 digests in structured JSON format.

    CrossShell for UNIX                                                      sha256sum(1)
    )";
    }

    static void printVersion() {
        std::cout << "sha256sum 1.0.0\n";
    }

    static bool parse(int argc, char* argv[], Sha256Options& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "--version") {
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
                std::cerr << "sha256sum: invalid option '" << arg << "'\n";
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

struct Sha256Result {
    std::string filename;
    std::string hash;
    bool isBinary{true};
    bool error{false};
};

class Sha256Reporter {
public:
    static int dispatch(const std::vector<Sha256Result>& results, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"sha256\":[";
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
            text = "HASH                                                              FILE\n----------------------------------------------------------------  ----\n";
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

class Sha256ChecksumVerifier {
public:
    static bool parseLine(const std::string& line, std::string& expectedHash, bool& isBinary, std::string& filename) {
        if (line.length() < 66) return false;

        expectedHash = line.substr(0, 64);
        for (char c : expectedHash) {
            if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
        }

        size_t idx = 64;
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

class Sha256Engine {
private:
    Sha256Options options;

    static std::string computeStream(std::istream& is) {
        SHA256DigestEngine sha;
        char buffer[65536];
        while (is.read(buffer, sizeof(buffer)) || is.gcount() > 0) {
            sha.update(reinterpret_cast<const uint8_t*>(buffer), static_cast<size_t>(is.gcount()));
        }
        return sha.finalize();
    }

public:
    explicit Sha256Engine(Sha256Options opts) : options(std::move(opts)) {}

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
                        std::cerr << "sha256sum: " << file << ": No such file or directory\n";
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

                if (!Sha256ChecksumVerifier::parseLine(line, expectedHash, lineIsBinary, filename)) {
                    formatErrors++;
                    if (options.warn && !options.status) {
                        std::cerr << "sha256sum: " << file << ": " << lineNum << ": improperly formatted SHA256 checksum line\n";
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
                std::cerr << "sha256sum: WARNING: " << formatErrors << " line(s) improperly formatted\n";
            }
            if (readFailures > 0) {
                std::cerr << "sha256sum: WARNING: " << readFailures << " listed file(s) could not be read\n";
            }
            if (checksumMismatches > 0) {
                std::cerr << "sha256sum: WARNING: " << checksumMismatches << " computed checksum(s) did NOT match\n";
            }
        }

        return (checksumMismatches > 0 || readFailures > 0 || totalChecked == 0) ? 1 : 0;
    }

    int executeCompute() {
        int exitCode = 0;
        std::vector<Sha256Result> results;

        for (const auto& file : options.files) {
            bool err = false;
            std::string hash = computeFile(file, options.binaryMode, err);

            if (err) {
                std::cerr << "sha256sum: " << file << ": No such file or directory\n";
                exitCode = 1;
                results.push_back({ file, "", options.binaryMode, true });
            } else {
                results.push_back({ file, hash, options.binaryMode, false });
            }
        }

        Sha256Reporter::dispatch(results, options.outputFormat, options.pipeCommand);
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

class Sha256App {
public:
    static int run(int argc, char* argv[]) {
        Sha256Options options;
        if (!Sha256Options::parse(argc, argv, options)) {
            return 1;
        }
        Sha256Engine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return Sha256App::run(argc, argv);
}
