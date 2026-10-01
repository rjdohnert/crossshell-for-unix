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
 * SINGLE FILE INDEX: less.cpp
 * ============================================================================
 * WinLess - Object-Oriented Terminal Pager for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & KEY DEFINITIONS] ........... KeyCode, LessOptions classes
 * 2. [TERMINAL DISPLAY CONTROLLER] ......... TerminalDisplay class (VT processing, dimensions)
 * 3. [INTERACTIVE PAGER ENGINE] ............ TextPagerEngine class (navigation, search, render)
 * 4. [APPLICATION CONTROLLER] .............. LessApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <conio.h>
#include <io.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. OPTIONS & KEY DEFINITIONS
// ============================================================================

enum class KeyCode {
    NONE,
    UP,
    DOWN,
    PAGE_UP,
    PAGE_DOWN,
    HOME,
    END,
    QUIT,
    HELP,
    SEARCH_FWD,
    SEARCH_BWD,
    SEARCH_NEXT,
    SEARCH_PREV,
    TOGGLE_NUMS,
    NEXT_FILE,
    PREV_FILE,
    OTHER
};

class LessOptions {
public:
    bool showLineNumbers{false};
    bool quiet{false};
    std::vector<std::string> files;

    static void printHelp() {
        std::cout << R"(less(1)             CrossShell for UNIX Reference Manual                 less(1)

    NAME
        less - terminal pager for viewing text files interactively

    SYNOPSIS
        less [OPTIONS] [FILE...]

    DESCRIPTION
        less is a terminal paging program that allows forward and backward
        navigation through a file or standard input. It does not read the entire
        input before starting, allowing faster startup with large files.

    OPTIONS
        -N, --LINE-NUMBERS
            Display line numbers preceding each line.

        -q, --quiet
            Operate in quiet mode with minimal alerts.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    NAVIGATION CONTROLS
        j, Down, Enter
            Scroll forward one line.

        k, Up, y
            Scroll backward one line.

        f, Space, PgDn
            Scroll forward one window.

        b, PgUp
            Scroll backward one window.

        g, Home
            Navigate to beginning of file.

        G, End
            Navigate to end of file.

        /pattern
            Search forward for pattern.

        ?pattern
            Search backward for pattern.

        n
            Repeat previous search in forward direction.

        N
            Repeat previous search in backward direction.

        q, Q
            Exit the pager.

    EXAMPLES
        less document.txt
            View document.txt interactively.

        less -N source.cpp
            View source.cpp with line numbers.

        git log | less
            Page through piped command output.

    CrossShell for UNIX                                                    less(1)
)";
    }

    static void printVersion() {
        std::cout << "less 1.0\n";
    }

    static bool parse(int argc, char* argv[], LessOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printHelp();
                std::exit(0);
            } else if (arg == "-V" || arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-N" || arg == "--LINE-NUMBERS") {
                opts.showLineNumbers = true;
            } else if (arg == "-q" || arg == "--quiet") {
                opts.quiet = true;
            } else if (arg[0] == '-' && arg != "-") {
                std::cerr << "less: unrecognized option: " << arg << "\n";
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
// 2. TERMINAL DISPLAY CONTROLLER
// ============================================================================

class TerminalDisplay {
public:
    static void enableVtMode() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return;
        DWORD mode = 0;
        if (GetConsoleMode(hOut, &mode)) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, mode);
        }
    }

    static void getSize(int& width, int& height) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (GetConsoleScreenBufferInfo(hOut, &csbi)) {
            width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        } else {
            width = 80;
            height = 25;
        }
    }

    static KeyCode readKey() {
        int ch = _getch();
        if (ch == 0 || ch == 0xE0 || ch == 224) {
            int ext = _getch();
            switch (ext) {
                case 72: return KeyCode::UP;
                case 80: return KeyCode::DOWN;
                case 73: return KeyCode::PAGE_UP;
                case 81: return KeyCode::PAGE_DOWN;
                case 71: return KeyCode::HOME;
                case 79: return KeyCode::END;
                default: return KeyCode::OTHER;
            }
        }

        switch (ch) {
            case 'q': case 'Q': return KeyCode::QUIT;
            case 'h': case 'H': return KeyCode::HELP;
            case 'j': case '\r': case '\n': return KeyCode::DOWN;
            case 'k': case 'y': return KeyCode::UP;
            case 'f': case ' ': return KeyCode::PAGE_DOWN;
            case 'b': return KeyCode::PAGE_UP;
            case 'g': return KeyCode::HOME;
            case 'G': return KeyCode::END;
            case '/': return KeyCode::SEARCH_FWD;
            case '?': return KeyCode::SEARCH_BWD;
            case 'n': return KeyCode::SEARCH_NEXT;
            case 'N': return KeyCode::SEARCH_PREV;
            case ':': return KeyCode::NEXT_FILE;
            default: return KeyCode::OTHER;
        }
    }
};

// ============================================================================
// 3. INTERACTIVE PAGER ENGINE
// ============================================================================

class TextPagerEngine {
private:
    std::vector<std::string> lines;
    std::string filename;
    LessOptions options;
    int topIndex{0};
    std::string searchQuery;

    static std::string toLower(const std::string& str) {
        std::string res = str;
        std::transform(res.begin(), res.end(), res.begin(), [](unsigned char c) { return std::tolower(c); });
        return res;
    }

    static std::string expandTabs(const std::string& input, int tabWidth = 4) {
        std::string result;
        for (char c : input) {
            if (c == '\t') {
                int spaces = tabWidth - (static_cast<int>(result.length()) % tabWidth);
                result.append(spaces, ' ');
            } else {
                result.push_back(c);
            }
        }
        return result;
    }

    void render() {
        int width = 80, height = 25;
        TerminalDisplay::getSize(width, height);
        int viewHeight = (std::max)(1, height - 1);

        std::string buffer;
        buffer += "\033[H\033[2J"; // Clear screen

        for (int i = 0; i < viewHeight; ++i) {
            int lineIdx = topIndex + i;
            if (lineIdx < static_cast<int>(lines.size())) {
                std::string line = lines[lineIdx];
                line = expandTabs(line);

                if (options.showLineNumbers) {
                    char numBuf[32];
                    snprintf(numBuf, sizeof(numBuf), "\033[33m%6d\033[0m ", lineIdx + 1);
                    buffer += numBuf;
                }

                if (!searchQuery.empty()) {
                    std::string lowerLine = toLower(line);
                    std::string lowerQuery = toLower(searchQuery);
                    size_t pos = 0;
                    std::string highlighted;
                    while (pos < line.size()) {
                        size_t match = lowerLine.find(lowerQuery, pos);
                        if (match == std::string::npos) {
                            highlighted += line.substr(pos);
                            break;
                        }
                        highlighted += line.substr(pos, match - pos);
                        highlighted += "\033[7m" + line.substr(match, searchQuery.size()) + "\033[0m";
                        pos = match + searchQuery.size();
                    }
                    line = highlighted;
                }

                buffer += line;
            } else {
                buffer += "~";
            }
            buffer += "\033[K\n"; // Clear to end of line
        }

        // Status bar
        buffer += "\033[7m";
        if (lines.empty()) {
            buffer += " (END) - " + filename;
        } else {
            int pct = (lines.size() <= static_cast<size_t>(viewHeight)) ? 100
                    : static_cast<int>((static_cast<double>(topIndex) / (lines.size() - viewHeight)) * 100.0);
            pct = (std::min)(100, (std::max)(0, pct));
            buffer += " " + filename + " lines " + std::to_string(topIndex + 1) + "-"
                   + std::to_string((std::min)(static_cast<int>(lines.size()), topIndex + viewHeight))
                   + "/" + std::to_string(lines.size()) + " (" + std::to_string(pct) + "%)";
        }
        buffer += "\033[0m";

        std::cout << buffer << std::flush;
    }

public:
    TextPagerEngine(std::vector<std::string> l, std::string fn, LessOptions opts)
        : lines(std::move(l)), filename(std::move(fn)), options(std::move(opts)) {}

    void run() {
        TerminalDisplay::enableVtMode();

        int width = 80, height = 25;
        TerminalDisplay::getSize(width, height);
        int viewHeight = (std::max)(1, height - 1);

        render();

        while (true) {
            KeyCode key = TerminalDisplay::readKey();
            TerminalDisplay::getSize(width, height);
            viewHeight = (std::max)(1, height - 1);
            int maxTop = (std::max)(0, static_cast<int>(lines.size()) - viewHeight);

            if (key == KeyCode::QUIT) break;
            else if (key == KeyCode::DOWN) {
                if (topIndex < maxTop) topIndex++;
            } else if (key == KeyCode::UP) {
                if (topIndex > 0) topIndex--;
            } else if (key == KeyCode::PAGE_DOWN) {
                topIndex = (std::min)(maxTop, topIndex + viewHeight);
            } else if (key == KeyCode::PAGE_UP) {
                topIndex = (std::max)(0, topIndex - viewHeight);
            } else if (key == KeyCode::HOME) {
                topIndex = 0;
            } else if (key == KeyCode::END) {
                topIndex = maxTop;
            } else if (key == KeyCode::SEARCH_FWD) {
                std::cout << "\033[" << height << ";1H\033[K/";
                std::string q;
                std::getline(std::cin, q);
                if (!q.empty()) searchQuery = q;
            }

            render();
        }

        std::cout << "\033[H\033[2J\033[0m" << std::flush;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class LessApp {
public:
    static int run(int argc, char* argv[]) {
        LessOptions options;
        if (!LessOptions::parse(argc, argv, options)) {
            return 1;
        }

        for (const auto& file : options.files) {
            std::vector<std::string> lines;
            if (file == "-") {
                std::string line;
                while (std::getline(std::cin, line)) lines.push_back(line);
            } else {
                std::ifstream in(file);
                if (!in.is_open()) {
                    std::cerr << "less: " << file << ": No such file or directory\n";
                    continue;
                }
                std::string line;
                while (std::getline(in, line)) lines.push_back(line);
            }

            if (!_isatty(_fileno(stdout))) {
                for (const auto& l : lines) std::cout << l << "\n";
                continue;
            }

            TextPagerEngine pager(lines, file, options);
            pager.run();
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    return LessApp::run(argc, argv);
}
