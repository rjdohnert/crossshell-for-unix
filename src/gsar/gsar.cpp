/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, Roberto J Dohnert
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * Neither the name of the project nor the names of its contributors may be
 * used to endorse or promote products derived from this software without
 * specific prior written permission.
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

#include <iostream>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <filesystem>
#include <cstdint>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <algorithm>

#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#define SET_BINARY_MODE(handle) _setmode(_fileno(handle), _O_BINARY)
#else
#define SET_BINARY_MODE(handle) ((void)0)
#endif

namespace fs = std::filesystem;

// ============================================================================
// Pattern / Byte Parser
// ============================================================================
class PatternParser {
public:
    static std::vector<uint8_t> parse(std::string_view input) {
        std::vector<uint8_t> bytes;
        bytes.reserve(input.size());

        for (size_t i = 0; i < input.size(); ++i) {
            if (input[i] == '\\' && i + 1 < input.size()) {
                ++i;
                switch (input[i]) {
                    case 'a':  bytes.push_back(0x07); break; // Bell
                    case 'b':  bytes.push_back(0x08); break; // Backspace
                    case 'e':  bytes.push_back(0x1B); break; // Escape
                    case 'f':  bytes.push_back(0x0C); break; // Form feed
                    case 'n':  bytes.push_back(0x0A); break; // Line feed
                    case 'r':  bytes.push_back(0x0D); break; // Carriage return
                    case 't':  bytes.push_back(0x09); break; // Horizontal tab
                    case 'v':  bytes.push_back(0x0B); break; // Vertical tab
                    case '\\': bytes.push_back('\\'); break;
                    case '0': { // Octal: \0OOO
                        size_t count = 0;
                        unsigned int val = 0;
                        while (i + 1 < input.size() && count < 3 && input[i + 1] >= '0' && input[i + 1] <= '7') {
                            val = (val * 8) + (input[++i] - '0');
                            count++;
                        }
                        bytes.push_back(static_cast<uint8_t>(val & 0xFF));
                        break;
                    }
                    case 'x': case 'X': { // Hex: \xHH
                        std::string hexStr;
                        while (i + 1 < input.size() && hexStr.size() < 2 && std::isxdigit(static_cast<unsigned char>(input[i + 1]))) {
                            hexStr.push_back(input[++i]);
                        }
                        if (!hexStr.empty()) {
                            bytes.push_back(static_cast<uint8_t>(std::stoul(hexStr, nullptr, 16)));
                        } else {
                            bytes.push_back(static_cast<uint8_t>('x'));
                        }
                        break;
                    }
                    case 'd': case 'D': { // Decimal: \dDDD
                        std::string decStr;
                        while (i + 1 < input.size() && decStr.size() < 3 && std::isdigit(static_cast<unsigned char>(input[i + 1]))) {
                            decStr.push_back(input[++i]);
                        }
                        if (!decStr.empty()) {
                            bytes.push_back(static_cast<uint8_t>(std::stoul(decStr, nullptr, 10)));
                        } else {
                            bytes.push_back(static_cast<uint8_t>('d'));
                        }
                        break;
                    }
                    default:
                        bytes.push_back(static_cast<uint8_t>(input[i]));
                        break;
                }
            } else {
                bytes.push_back(static_cast<uint8_t>(input[i]));
            }
        }
        return bytes;
    }

    static std::string toHexPreview(const std::vector<uint8_t>& data, size_t maxLen = 16) {
        std::ostringstream oss;
        size_t len = std::min(data.size(), maxLen);
        for (size_t i = 0; i < len; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
        }
        if (data.size() > maxLen) oss << "...";
        return oss.str();
    }
};

// ============================================================================
// Application Configuration & Options
// ============================================================================
struct Options {
    std::vector<uint8_t> searchPattern;
    std::vector<uint8_t> replacePattern;
    bool hasSearch = false;
    bool hasReplace = false;
    bool caseInsensitive = false;
    bool countOnly = false;
    bool verbose = false;
    bool showOffsets = false;
    bool createBackup = false;
    bool filterPipeMode = false;
    bool displayHex = false;
    std::string backupExtension = ".bak";
    std::vector<std::string> inputFiles;
    std::string outputFile;
};

// ============================================================================
// Command-Line Parser & Help System
// ============================================================================
class CommandLineParser {
public:
    static void printHelp(const std::string& exeName) {
           std::cout << R"(gsar(1)                 CrossShell for UNIX Reference Manual                     gsar(1)

    NAME
        gsar - general binary and text search-and-replace utility

    SYNOPSIS
        )" << exeName << R"( -s<pattern> [-r<pattern>] [OPTIONS] [INFILE...] [OUTFILE]
        type INPUT | )" << exeName << R"( -s<pattern> -r<pattern> > OUTPUT

    DESCRIPTION
        GSAR searches and replaces patterns in binary data and text files. It can
        modify files in place, search arbitrary binary streams, count occurrences,
        display match contexts, or operate as a filter in Windows CMD and PowerShell
        pipelines.

    OPTIONS
        -s<string>
            Search string or pattern (for example, -s"foo" or -s"\x0D\x0A").

        -r[string]
            Replacement string or pattern. Use -r"" to delete matches.

        -i
            Use case-insensitive ASCII byte matching.

        -c
            Count occurrences only; disable output streaming and replacement.

        -b
            Display match byte offsets in hexadecimal.

        -v
            Enable verbose output summarizing files, matches, and actions.

        -x
            Display matched contexts in hexadecimal dump format.

        -f
            Force pipe/filter mode, reading stdin and writing stdout.

        -u
            Create a backup file with the .bak extension before modifying a file.

        -B<ext>
            Set a custom backup extension (for example, -B.old).

        -o<file>
            Explicitly set the target output file.

        -h, --help, /?
            Display this comprehensive reference manual and exit.

    ESCAPE SEQUENCES
        The following escape sequences are supported in -s and -r patterns:

        \a       Bell or alert (0x07)
        \b       Backspace (0x08)
        \e       Escape (0x1B)
        \f       Form feed (0x0C)
        \n       Newline or line feed (0x0A)
        \r       Carriage return (0x0D)
        	       Horizontal tab (0x09)
        \v       Vertical tab (0x0B)
        \\       Literal backslash
        \0OOO    Octal value, up to 3 octal digits
        \xHH     Hexadecimal value, exactly 2 hex digits
        \dDDD    Decimal value, up to 3 decimal digits

    PIPELINE AND REDIRECTION
        When no input files are specified, or when '-' or -f is provided, gsar reads
        from standard input and writes to standard output. Binary modes are enabled
        automatically to prevent Windows CRLF translation from changing the data.

    EXAMPLES
        )" << exeName << R"( -s"\r\n" -r"\n" script.sh
            Replace Windows CRLF with Unix LF in place.

        type unix.txt | )" << exeName << R"( -s"\n" -r"\r\n" > win.txt
            Convert Unix LF to Windows CRLF in a pipeline.

        )" << exeName << R"( -s"\x4D\x5A\x90\x00" -b -x binary.exe
            Search for a binary signature in an executable.

        )" << exeName << R"( -s"database_v1" -r"database_v2" -i -u config.xml
            Replace text case-insensitively and create a backup.

        )" << exeName << R"( -s"ERROR_FAILED" -c application.log
            Count occurrences of a specific pattern.

    CrossShell for UNIX                                                       gsar(1)
    )";
    }

    static Options parse(int argc, char* argv[]) {
        Options opt;
        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printHelp(fs::path(argv[0]).stem().string());
                std::exit(0);
            } else if (arg.rfind("-s", 0) == 0) {
                std::string pat = arg.substr(2);
                if (pat.empty() && i + 1 < argc && argv[i + 1][0] != '-') {
                    pat = argv[++i];
                }
                opt.searchPattern = PatternParser::parse(pat);
                opt.hasSearch = true;
            } else if (arg.rfind("-r", 0) == 0) {
                std::string pat = arg.substr(2);
                if (pat.empty() && i + 1 < argc && argv[i + 1][0] != '-') {
                    pat = argv[++i];
                }
                opt.replacePattern = PatternParser::parse(pat);
                opt.hasReplace = true;
            } else if (arg == "-i") {
                opt.caseInsensitive = true;
            } else if (arg == "-c") {
                opt.countOnly = true;
            } else if (arg == "-v") {
                opt.verbose = true;
            } else if (arg == "-b") {
                opt.showOffsets = true;
            } else if (arg == "-x") {
                opt.displayHex = true;
            } else if (arg == "-f") {
                opt.filterPipeMode = true;
            } else if (arg == "-u") {
                opt.createBackup = true;
            } else if (arg.rfind("-B", 0) == 0) {
                opt.createBackup = true;
                opt.backupExtension = arg.substr(2);
                if (opt.backupExtension.empty() && i + 1 < argc) {
                    opt.backupExtension = argv[++i];
                }
            } else if (arg.rfind("-o", 0) == 0) {
                opt.outputFile = arg.substr(2);
                if (opt.outputFile.empty() && i + 1 < argc) {
                    opt.outputFile = argv[++i];
                }
            } else if (arg.front() == '-') {
                std::cerr << "[gsar] Warning: Unknown option ignored: " << arg << "\n";
            } else {
                positional.push_back(arg);
            }
        }

        if (!opt.hasSearch) {
            std::cerr << "[gsar] Error: Search pattern (-s<string>) is required.\n";
            std::cerr << "       Use -h or --help for detailed usage information.\n";
            std::exit(1);
        }

        // Handle positional input/output files
        if (!positional.empty()) {
            if (positional.size() == 2 && opt.outputFile.empty() && !opt.filterPipeMode) {
                opt.inputFiles.push_back(positional[0]);
                opt.outputFile = positional[1];
            } else {
                for (const auto& file : positional) {
                    opt.inputFiles.push_back(file);
                }
            }
        }

        return opt;
    }
};

// ============================================================================
// Core Search & Replace Engine
// ============================================================================
class SearchEngine {
private:
    Options options;
    static constexpr size_t CHUNK_SIZE = 64 * 1024; // 64 KB buffer

    bool matchesAt(const uint8_t* buffer, size_t pos, size_t bufferSize) const {
        if (pos + options.searchPattern.size() > bufferSize) return false;
        
        for (size_t i = 0; i < options.searchPattern.size(); ++i) {
            uint8_t a = buffer[pos + i];
            uint8_t b = options.searchPattern[i];
            if (options.caseInsensitive) {
                if (std::tolower(static_cast<unsigned char>(a)) != std::tolower(static_cast<unsigned char>(b))) {
                    return false;
                }
            } else {
                if (a != b) return false;
            }
        }
        return true;
    }

    void displayHexContext(uint64_t offset, const uint8_t* data, size_t length) const {
        std::cout << "[Match at 0x" << std::hex << std::setw(8) << std::setfill('0') << offset << "] ";
        for (size_t i = 0; i < length; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]) << " ";
        }
        std::cout << " | ";
        for (size_t i = 0; i < length; ++i) {
            char c = static_cast<char>(data[i]);
            std::cout << (std::isprint(static_cast<unsigned char>(c)) ? c : '.');
        }
        std::cout << std::dec << "\n";
    }

public:
    explicit SearchEngine(Options opt) : options(std::move(opt)) {}

    struct Result {
        uint64_t matchCount = 0;
        uint64_t bytesRead = 0;
        uint64_t bytesWritten = 0;
    };

    Result processStream(std::istream& in, std::ostream* out) {
        Result res;
        const size_t patSize = options.searchPattern.size();
        
        if (patSize == 0) return res;

        std::vector<uint8_t> buffer;
        buffer.reserve(CHUNK_SIZE + patSize);
        std::vector<uint8_t> readChunk(CHUNK_SIZE);

        uint64_t streamOffset = 0;

        while (in) {
            in.read(reinterpret_cast<char*>(readChunk.data()), CHUNK_SIZE);
            std::streamsize bytesJustRead = in.gcount();
            if (bytesJustRead <= 0) break;

            res.bytesRead += bytesJustRead;
            buffer.insert(buffer.end(), readChunk.begin(), readChunk.begin() + bytesJustRead);

            size_t i = 0;
            // Process buffer while we have enough room to check pattern
            while (i + patSize <= buffer.size()) {
                if (matchesAt(buffer.data(), i, buffer.size())) {
                    res.matchCount++;
                    uint64_t matchAbsoluteOffset = streamOffset + i;

                    if (options.showOffsets) {
                        std::cout << "Match at offset: 0x" << std::hex << matchAbsoluteOffset << std::dec << "\n";
                    }
                    if (options.displayHex) {
                        displayHexContext(matchAbsoluteOffset, buffer.data() + i, patSize);
                    }

                    if (out && options.hasReplace && !options.countOnly) {
                        if (!options.replacePattern.empty()) {
                            out->write(reinterpret_cast<const char*>(options.replacePattern.data()), options.replacePattern.size());
                            res.bytesWritten += options.replacePattern.size();
                        }
                    }
                    i += patSize;
                } else {
                    if (out && !options.countOnly) {
                        out->put(static_cast<char>(buffer[i]));
                        res.bytesWritten++;
                    }
                    i++;
                }
            }

            streamOffset += i;
            // Keep remaining tail of the buffer for the next sliding overlap
            buffer.erase(buffer.begin(), buffer.begin() + i);
        }

        // Flush remaining buffer bytes that could not form a full match
        if (out && !options.countOnly && !buffer.empty()) {
            out->write(reinterpret_cast<const char*>(buffer.data()), buffer.size());
            res.bytesWritten += buffer.size();
        }

        return res;
    }
};

// ============================================================================
// Main Application Controller
// ============================================================================
class GSARApp {
private:
    Options options;

    void processFile(const std::string& inFilePath, const std::string& outFilePath) {
        fs::path inputPath(inFilePath);
        if (!fs::exists(inputPath)) {
            std::cerr << "[gsar] Error: Input file does not exist: " << inFilePath << "\n";
            return;
        }

        bool inPlace = outFilePath.empty() || (fs::absolute(inputPath) == fs::absolute(fs::path(outFilePath)));

        if (options.verbose) {
            std::cout << "[gsar] Processing file: " << inFilePath << (inPlace ? " (in-place)" : " -> " + outFilePath) << "\n";
        }

        std::ifstream inFile(inFilePath, std::ios::binary);
        if (!inFile) {
            std::cerr << "[gsar] Error: Unable to open input file: " << inFilePath << "\n";
            return;
        }

        SearchEngine engine(options);
        SearchEngine::Result res;

        if (options.countOnly || (!options.hasReplace && outFilePath.empty())) {
            // Read-only inspection
            res = engine.processStream(inFile, nullptr);
        } else if (inPlace) {
            // In-place replacement via temporary file
            fs::path tempPath = inputPath.parent_path() / (inputPath.filename().string() + ".gsar_tmp");
            {
                std::ofstream tempOut(tempPath, std::ios::binary);
                if (!tempOut) {
                    std::cerr << "[gsar] Error: Cannot create temporary file: " << tempPath.string() << "\n";
                    return;
                }
                res = engine.processStream(inFile, &tempOut);
            }
            inFile.close();

            if (options.createBackup) {
                fs::path backupPath = inputPath.string() + options.backupExtension;
                std::error_code ec;
                fs::copy_file(inputPath, backupPath, fs::copy_options::overwrite_existing, ec);
                if (ec && options.verbose) {
                    std::cerr << "[gsar] Warning: Failed to create backup file: " << ec.message() << "\n";
                }
            }

            std::error_code ec;
            fs::rename(tempPath, inputPath, ec);
            if (ec) {
                // If rename failed (e.g. across drives), fallback to copy & remove
                fs::copy_file(tempPath, inputPath, fs::copy_options::overwrite_existing, ec);
                fs::remove(tempPath, ec);
            }
        } else {
            // Different output file
            std::ofstream outFile(outFilePath, std::ios::binary);
            if (!outFile) {
                std::cerr << "[gsar] Error: Unable to open output file: " << outFilePath << "\n";
                return;
            }
            res = engine.processStream(inFile, &outFile);
        }

        if (options.countOnly || options.verbose) {
            std::cout << inFilePath << ": " << res.matchCount << " occurrence(s) found.\n";
        }
    }

public:
    explicit GSARApp(Options opt) : options(std::move(opt)) {}

    int run() {
        SET_BINARY_MODE(stdin);
        SET_BINARY_MODE(stdout);

        // Pipe / Filter mode
        if (options.filterPipeMode || (options.inputFiles.empty() && options.outputFile.empty())) {
            SearchEngine engine(options);
            SearchEngine::Result res = engine.processStream(std::cin, &std::cout);
            if (options.countOnly) {
                std::cerr << "Matches: " << res.matchCount << "\n";
            }
            return 0;
        }

        // File processing mode
        if (!options.outputFile.empty() && options.inputFiles.size() == 1) {
            processFile(options.inputFiles[0], options.outputFile);
        } else {
            for (const auto& file : options.inputFiles) {
                processFile(file, "");
            }
        }

        return 0;
    }
};

// ============================================================================
// Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    try {
        Options options = CommandLineParser::parse(argc, argv);
        GSARApp app(std::move(options));
        return app.run();
    } catch (const std::exception& ex) {
        std::cerr << "[gsar] Fatal Exception: " << ex.what() << "\n";
        return 1;
    }
}