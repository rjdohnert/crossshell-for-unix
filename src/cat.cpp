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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <sstream>
#include <memory>
#include <algorithm>

// ============================================================================
// Class: JsonSerializer
// Simple JSON helper for escaping and formatting (no external libs)
// ============================================================================
class JsonSerializer {
public:
    static std::string escape(const std::string& str) {
        std::ostringstream oss;
        for (unsigned char c : str) {
            switch (c) {
                case '"': oss << "\\\""; break;
                case '\\': oss << "\\\\"; break;
                case '\b': oss << "\\b"; break;
                case '\f': oss << "\\f"; break;
                case '\n': oss << "\\n"; break;
                case '\r': oss << "\\r"; break;
                case '\t': oss << "\\t"; break;
                default:
                    if (c < 32) oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(c);
                    else oss.put(c);
            }
        }
        return oss.str();
    }
};

// ============================================================================
// Class: CatOptions
// Holds HP-UX configuration flags: -b, -e, -n, -r, -s, -t, -u, -v.
// ============================================================================
class CatOptions {
public:
    bool numberLines = false;      // -n: Precede lines by line numbers
    bool numberNonBlank = false;   // -b: Omit line numbers from blank lines (implies -n)
    bool showEnds = false;         // -e: Display $ before newlines (implies -v)
    bool squeezeBlanks = false;    // -r: Squeeze consecutive blank lines into one
    bool silent = false;           // -s: Silent about non-existent files and read errors
    bool showTabs = false;         // -t: Display tabs as ^I, form feeds as ^L (implies -v)
    bool unbuffered = false;       // -u: Handle output character-by-character
    bool showNonPrinting = false;  // -v: Visibly display control & non-ASCII characters
    bool outputJson = false;       // --json: Output structured JSON

    std::vector<std::string> files;

    void normalizeDependencies() {
        if (numberNonBlank) {
            numberLines = true;
        }
        if (showEnds || showTabs) {
            showNonPrinting = true;
        }
    }

    bool requiresFormatting() const {
        return outputJson || numberLines || showEnds || squeezeBlanks || showTabs || showNonPrinting;
    }
};

// ============================================================================
// Class: CharacterFormatter
// Implements the HP-UX -v, -t, and -e non-printable character conversions.
// ============================================================================
class CharacterFormatter {
public:
    static void formatAndPrintChar(unsigned char c, bool showTabs, bool showNonPrinting) {
        if (c == '\t') {
            if (showTabs) {
                std::cout << "^I";
            } else {
                std::cout << '\t';
            }
            return;
        }

        if (c == '\f') {
            if (showTabs) {
                std::cout << "^L";
            } else {
                std::cout << '\f';
            }
            return;
        }

        if (!showNonPrinting) {
            std::cout.put(static_cast<char>(c));
            return;
        }

        // Control characters: 0 - 31 (except \t and \n)
        if (c < 32 && c != '\n') {
            std::cout << '^' << static_cast<char>(c + 64);
        }
        // DEL character: 127
        else if (c == 127) {
            std::cout << "^?";
        }
        // High-bit non-ASCII: 128 - 255
        else if (c >= 128) {
            std::cout << "M-";
            unsigned char low7 = c & 0x7F;
            if (low7 < 32) {
                std::cout << '^' << static_cast<char>(low7 + 64);
            } else if (low7 == 127) {
                std::cout << "^?";
            } else {
                std::cout.put(static_cast<char>(low7));
            }
        } else {
            std::cout.put(static_cast<char>(c));
        }
    }
};

// ============================================================================
// Class: StreamProcessor
// Handles chunked pass-through I/O or line-by-line formatting.
// ============================================================================
class StreamProcessor {
private:
    const CatOptions& options;
    uint64_t currentLineNumber = 1;
    bool previousWasBlank = false;

    void printLineNumber() {
        std::cout << std::right << std::setw(6) << currentLineNumber++ << "\t";
    }

public:
    explicit StreamProcessor(const CatOptions& opts) : options(opts) {}

    // Fast block transfer for unadorned streams
    void processRaw(std::istream& in) {
        constexpr size_t BUF_SIZE = 64 * 1024;
        std::vector<char> buffer(BUF_SIZE);

        while (in.read(buffer.data(), buffer.size()) || in.gcount() > 0) {
            std::cout.write(buffer.data(), in.gcount());
            if (options.unbuffered) {
                std::cout.flush();
            }
        }
    }

    // Formatted stream processor for HP-UX -benrtv flags
    void processFormatted(std::istream& in) {
        std::string line;
        bool atLineStart = true;

        char ch;
        std::vector<unsigned char> lineBuffer;

        while (in.get(ch)) {
            unsigned char uc = static_cast<unsigned char>(ch);

            // Strip Windows Carriage Returns to preserve line consistency
            if (uc == '\r') {
                continue;
            }

            if (uc != '\n') {
                lineBuffer.push_back(uc);
                continue;
            }

            // Encountered newline: evaluate line content
            bool isBlank = lineBuffer.empty();

            if (options.squeezeBlanks && isBlank && previousWasBlank) {
                lineBuffer.clear();
                continue; // Squeeze multiple blanks into one
            }

            previousWasBlank = isBlank;

            if (options.outputJson) {
                // Output as JSON object
                std::string content;
                for (unsigned char c : lineBuffer) {
                    content.push_back(static_cast<char>(c));
                }
                std::cout << "{\"line\":" << currentLineNumber << ",\"content\":\"" 
                          << JsonSerializer::escape(content) << "\"}\n";
                currentLineNumber++;
            } else {
                // Output line numbers
                if (options.numberLines) {
                    if (options.numberNonBlank && isBlank) {
                        // Suppress line number on blank line
                    } else {
                        printLineNumber();
                    }
                }

                // Print line characters
                for (unsigned char c : lineBuffer) {
                    CharacterFormatter::formatAndPrintChar(c, options.showTabs, options.showNonPrinting);
                }

                // End-of-line marker
                if (options.showEnds) {
                    std::cout << '$';
                }

                std::cout.put('\n');

                if (options.unbuffered) {
                    std::cout.flush();
                }
            }
            lineBuffer.clear();
        }

        // Handle trailing data without newline
        if (!lineBuffer.empty()) {
            if (options.outputJson) {
                std::string content;
                for (unsigned char c : lineBuffer) {
                    content.push_back(static_cast<char>(c));
                }
                std::cout << "{\"line\":" << currentLineNumber << ",\"content\":\"" 
                          << JsonSerializer::escape(content) << "\"}\n";
            } else {
                if (options.numberLines) {
                    printLineNumber();
                }
                for (unsigned char c : lineBuffer) {
                    CharacterFormatter::formatAndPrintChar(c, options.showTabs, options.showNonPrinting);
                }
                if (options.unbuffered) {
                    std::cout.flush();
                }
            }
        }
    }
};

// ============================================================================
// Class: HelpFormatter
// HP-UX formatted Reference Manual page.
// ============================================================================
class HelpFormatter {
public:
    static void printHelp() {
        std::cout << R"(cat(1)                  CrossShell for UNIX Reference Manual                  cat(1)

    NAME
        cat - concatenate and display files

    SYNOPSIS
        cat [OPTIONS] [FILE...]

    DESCRIPTION
        cat reads each FILE in sequence and writes it to standard output.
        If FILE is omitted or specified as '-', cat reads from standard input.

    OPTIONS
        -b
            Number non-empty output lines, overriding -n.

        -e
            Display a '$' at end of each line (implies -v).

        -n
            Number all output lines starting at 1.

        -r
            Squeeze multiple adjacent empty lines into a single blank line.

        -s
            Suppress error messages about nonexistent or unreadable files.

        -t
            Display TAB characters as '^I' and form feeds as '^L' (implies -v).

        -u
            Disable output buffering for character-by-character processing.

        -v
            Display non-printing characters visibly using caret notation.

        --json
            Emit structured output records in JSON format.

        -h, --help
            Display this reference manual.

    EXAMPLES
        cat file1.txt file2.txt
            Concatenate and print files to standard output.

        cat -n source.cpp
            Display file with numbered lines.

        cat -ben main.c
            Display line numbers and ends on non-empty lines.

    CrossShell for UNIX                                                    cat(1)
)";
    }
};

// ============================================================================
// Class: ArgumentParser
// Parses HP-UX compound parameters (e.g., -ben, -vte, -rs).
// ============================================================================
class ArgumentParser {
public:
    static bool parse(int argc, char* argv[], CatOptions& options) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h") {
                HelpFormatter::printHelp();
                exit(0);
            }
            if (arg == "--json") {
                options.outputJson = true;
                continue;
            }

            if (arg == "-") {
                options.files.push_back("-");
                continue;
            }

            if (arg.length() > 1 && arg[0] == '-') {
                for (size_t c = 1; c < arg.length(); ++c) {
                    switch (arg[c]) {
                        case 'b': options.numberNonBlank = true; break;
                        case 'e': options.showEnds = true; break;
                        case 'n': options.numberLines = true; break;
                        case 'r': options.squeezeBlanks = true; break;
                        case 's': options.silent = true; break;
                        case 't': options.showTabs = true; break;
                        case 'u': options.unbuffered = true; break;
                        case 'v': options.showNonPrinting = true; break;
                        default:
                            std::cerr << "cat: illegal option -- " << arg[c] << "\n"
                                      << "usage: cat [-benrstuv] [-] [file ...]\n";
                            return false;
                    }
                }
            } else {
                options.files.push_back(arg);
            }
        }

        if (options.files.empty()) {
            options.files.push_back("-");
        }

        options.normalizeDependencies();
        return true;
    }
};

// ============================================================================
// Class: CatEngine
// Manages file descriptors, I/O streaming, and silent mode execution.
// ============================================================================
class CatEngine {
private:
    CatOptions options;
    StreamProcessor processor;

public:
    explicit CatEngine(const CatOptions& opts) : options(opts), processor(opts) {}

    int execute() {
        int exitCode = 0;

        for (const auto& filename : options.files) {
            if (filename == "-") {
                if (options.requiresFormatting()) {
                    processor.processFormatted(std::cin);
                } else {
                    processor.processRaw(std::cin);
                }
            } else {
                std::ifstream file(filename, std::ios::binary);
                if (!file.is_open()) {
                    if (!options.silent) {
                        std::cerr << "cat: cannot open " << filename << ": No such file or directory\n";
                    }
                    exitCode = 1;
                    continue;
                }

                if (options.requiresFormatting()) {
                    processor.processFormatted(file);
                } else {
                    processor.processRaw(file);
                }
                file.close();
            }
        }

        return exitCode;
    }
};

// ============================================================================
// Main Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    // Put stdin and stdout into binary mode on Windows to prevent CRLF corruption
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);

    CatOptions options;
    if (!ArgumentParser::parse(argc, argv, options)) {
        return 1;
    }

    if (options.unbuffered) {
        std::setvbuf(stdout, nullptr, _IONBF, 0);
    }

    CatEngine engine(options);
    return engine.execute();
}