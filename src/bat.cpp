/*
 * BSD 3-Clause License
 *
 * Copyright (c) 2026, cmd-extended contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
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
 * SINGLE FILE INDEX: bat.cpp
 * ============================================================================
 * WinBat - Object-Oriented Syntax Highlighting Cat Clone for Windows
 * Specification: C++17 | Platform: Windows NT (x86_64 / ARM64)
 *
 * TABLE OF CONTENTS:
 * 1. [OPTIONS & HIGHLIGHT STATE] ........... HighlightState and BatOptions classes
 * 2. [STRUCTURED OUTPUT REPORTER] .......... BatReporter class (JSON/CSV/Table/Pipe)
 * 3. [SYNTAX HIGHLIGHTER ENGINE] ........... SyntaxHighlighter and BatEngine classes
 * 4. [APPLICATION CONTROLLER] .............. BatApp class and main entry point
 * ============================================================================
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <unordered_set>
#include <filesystem>
#include <cctype>
#include <cstdio>
#include <memory>

namespace fs = std::filesystem;

namespace BatColor {
    const std::string RESET     = "\033[0m";
    const std::string BOLD      = "\033[1m";
    const std::string DIM       = "\033[2m";
    const std::string GRAY      = "\033[90m";
    const std::string RED       = "\033[31m";
    const std::string GREEN     = "\033[32m";
    const std::string YELLOW    = "\033[33m";
    const std::string BLUE      = "\033[34m";
    const std::string MAGENTA   = "\033[35m";
    const std::string CYAN      = "\033[36m";
    const std::string WHITE     = "\033[37m";
    const std::string B_CYAN    = "\033[96m";
    const std::string B_YELLOW  = "\033[93m";
}

// ============================================================================
// 1. OPTIONS & HIGHLIGHT STATE
// ============================================================================

struct HighlightState {
    bool inBlockComment{false};
    bool inTripleDoubleQuote{false};
    std::string blockTerminator;
    char stringDelimiter{0};
};

class BatOptions {
public:
    bool showLineNumbers{true};
    bool showGrid{true};
    bool showHeader{true};
    bool plain{false};
    bool showAll{false};
    bool forceBinaryDisplay{false};
    std::string forcedLanguage{""};
    std::vector<std::string> files;
    int outputFormat{0};
    std::string pipeCommand;

    static void printUsage(const char* prog = nullptr) {
        (void)prog;
        std::cout << R"(bat(1)                  CrossShell for UNIX Reference Manual                  bat(1)

    NAME
        bat - syntax-highlighted file viewer with Git-style decorations

    SYNOPSIS
        bat [OPTIONS] [FILE...]

    DESCRIPTION
        A cat clone with syntax highlighting and Git-style decorations.
        When FILE is omitted, or FILE is -, read standard input.

    OPTIONS
        -A, --show-all
            Show non-printable characters such as tabs, spaces, and newlines.

        -p, --plain
            Show plain style without line numbers, grid, or header.

        -n, --number
            Show only line numbers without grid or header.

        -l, --language LANGUAGE
            Explicitly set the syntax language.

        --json, --csv, --table
            Emit structured output in JSON, CSV, or tabular format.

        --pipe COMMAND
            Send output through COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        bat file.cpp
            Display syntax-highlighted file.

        bat -n script.sh
            Display file with line numbers only.

        bat --language python app.py
            Force Python syntax highlighting.

    CrossShell for UNIX                                                    bat(1)
)";
    }

    static void printVersion() {
        std::cout << "bat 1.0.0\n";
    }

    static bool parse(int argc, char* argv[], BatOptions& opts) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-h" || arg == "--help" || arg == "/?") {
                printUsage(argv[0]);
                std::exit(0);
            } else if (arg == "-V" || arg == "--version") {
                printVersion();
                std::exit(0);
            } else if (arg == "-p" || arg == "--plain") {
                opts.plain = true;
                opts.showLineNumbers = false;
                opts.showGrid = false;
                opts.showHeader = false;
            } else if (arg == "-n" || arg == "--number") {
                opts.showLineNumbers = true;
                opts.showGrid = false;
                opts.showHeader = false;
            } else if (arg == "-A" || arg == "--show-all") {
                opts.showAll = true;
            } else if ((arg == "-l" || arg == "--language") && i + 1 < argc) {
                opts.forcedLanguage = argv[++i];
            } else if (arg == "--json") {
                opts.outputFormat = 1;
            } else if (arg == "--csv") {
                opts.outputFormat = 2;
            } else if (arg == "--table") {
                opts.outputFormat = 3;
            } else if (arg == "--pipe" && i + 1 < argc) {
                opts.pipeCommand = argv[++i];
            } else if (arg[0] == '-' && arg != "-") {
                std::cerr << "bat: unknown option " << arg << "\n";
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
// 2. STRUCTURED OUTPUT REPORTER
// ============================================================================

class BatReporter {
public:
    static int dispatch(const std::string& content, int format, const std::string& pipeCommand) {
        std::string text;
        if (format == 1) {
            text = "{\"content\":\"" + content + "\"}\n";
        } else if (format == 2) {
            text = "content\n\"" + content + "\"\n";
        } else if (format == 3) {
            text = "CONTENT\n-------\n" + content + "\n";
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
// 3. SYNTAX HIGHLIGHTER ENGINE
// ============================================================================

enum class BatLanguage {
    Plain, Cpp, Go, Java, TypeScript, Cobol, Pascal, Html, Xml, Json, Python, Shell, Batch, Basic
};

class SyntaxHighlighter {
private:
    static std::string lower(std::string value) {
        for (char& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        return value;
    }

    static bool inSet(const std::string& word, BatLanguage language) {
        static const std::unordered_set<std::string> cKeywords = {
            "auto", "break", "case", "catch", "class", "const", "constexpr", "continue", "default", "delete", "do", "else", "enum", "for", "if", "namespace", "new", "nullptr", "private", "protected", "public", "return", "sizeof", "static", "struct", "switch", "template", "this", "throw", "try", "typedef", "true", "false", "typename", "using", "virtual", "volatile", "while"
        };
        static const std::unordered_set<std::string> goKeywords = {
            "break", "case", "chan", "const", "continue", "default", "defer", "else", "fallthrough", "for", "func", "go", "goto", "if", "import", "interface", "map", "package", "range", "return", "select", "struct", "switch", "type", "var", "true", "false", "nil"
        };
        static const std::unordered_set<std::string> javaKeywords = {
            "abstract", "assert", "boolean", "break", "byte", "case", "catch", "char", "class", "const", "continue", "default", "do", "double", "else", "enum", "extends", "final", "finally", "float", "for", "if", "implements", "import", "instanceof", "interface", "long", "native", "new", "package", "private", "protected", "public", "return", "short", "static", "strictfp", "super", "switch", "synchronized", "this", "throw", "throws", "transient", "try", "void", "volatile", "while", "true", "false", "null"
        };
        static const std::unordered_set<std::string> tsKeywords = {
            "abstract", "as", "any", "await", "boolean", "break", "case", "catch", "class", "const", "constructor", "continue", "debugger", "declare", "default", "delete", "do", "else", "enum", "export", "extends", "false", "finally", "for", "from", "function", "if", "implements", "import", "in", "infer", "instanceof", "interface", "keyof", "let", "module", "namespace", "never", "new", "null", "number", "object", "of", "package", "private", "protected", "public", "readonly", "return", "static", "string", "super", "switch", "symbol", "this", "throw", "true", "try", "type", "typeof", "undefined", "unknown", "var", "void", "while", "with", "yield"
        };
        static const std::unordered_set<std::string> pascalKeywords = {
            "array", "begin", "case", "const", "div", "do", "downto", "else", "end", "file", "for", "function", "goto", "if", "implementation", "in", "interface", "label", "mod", "nil", "not", "of", "or", "packed", "procedure", "program", "record", "repeat", "set", "string", "then", "to", "type", "unit", "until", "uses", "var", "while", "with", "and", "xor"
        };
        static const std::unordered_set<std::string> cobolKeywords = {
            "accept", "add", "call", "compute", "display", "divide", "else", "end-if", "end-perform", "end-read", "end-write", "evaluate", "exec", "file", "from", "goback", "if", "initialize", "inspect", "move", "multiply", "not", "open", "perform", "procedure", "program-id", "read", "rewrite", "stop", "string", "subtract", "then", "until", "write"
        };
        static const std::unordered_set<std::string> jsonKeywords = {"true", "false", "null"};
        static const std::unordered_set<std::string> scriptingKeywords = {
            "and", "as", "async", "await", "class", "def", "elif", "else", "except", "finally", "for", "from", "function", "if", "import", "in", "is", "lambda", "let", "not", "or", "pass", "print", "raise", "return", "self", "try", "var", "while", "with", "yield", "true", "false", "none", "null"
        };

        std::string value = lower(word);
        if (language == BatLanguage::Json) return jsonKeywords.find(value) != jsonKeywords.end();
        const std::unordered_set<std::string>* keywords = &cKeywords;
        if (language == BatLanguage::Go) keywords = &goKeywords;
        else if (language == BatLanguage::Java) keywords = &javaKeywords;
        else if (language == BatLanguage::TypeScript) keywords = &tsKeywords;
        else if (language == BatLanguage::Pascal) keywords = &pascalKeywords;
        else if (language == BatLanguage::Cobol) keywords = &cobolKeywords;
        else if (language == BatLanguage::Python || language == BatLanguage::Shell || language == BatLanguage::Basic) keywords = &scriptingKeywords;
        return keywords->find(value) != keywords->end();
    }

public:
    static BatLanguage detectLanguage(const std::string& label, const std::string& forced) {
        const std::string value = lower(forced.empty() ? fs::path(label).extension().string() : forced);
        if (value == ".c" || value == ".cc" || value == ".cpp" || value == ".cxx" || value == ".h" || value == ".hpp" || value == "c" || value == "cpp" || value == "c++") return BatLanguage::Cpp;
        if (value == ".go" || value == "go" || value == "golang") return BatLanguage::Go;
        if (value == ".java" || value == "java") return BatLanguage::Java;
        if (value == ".ts" || value == ".tsx" || value == "ts" || value == "typescript") return BatLanguage::TypeScript;
        if (value == ".cob" || value == ".cbl" || value == ".cpy" || value == "cobol" || value == "cob") return BatLanguage::Cobol;
        if (value == ".pas" || value == ".pp" || value == ".inc" || value == "pascal" || value == "pas") return BatLanguage::Pascal;
        if (value == ".html" || value == ".htm" || value == "html") return BatLanguage::Html;
        if (value == ".xml" || value == "xml") return BatLanguage::Xml;
        if (value == ".json" || value == "json") return BatLanguage::Json;
        if (value == ".py" || value == "python") return BatLanguage::Python;
        if (value == ".sh" || value == ".bash" || value == "shell") return BatLanguage::Shell;
        if (value == ".bat" || value == ".cmd" || value == "batch") return BatLanguage::Batch;
        if (value == ".bas" || value == "basic") return BatLanguage::Basic;
        return BatLanguage::Plain;
    }

    static bool initConsole() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return false;
        DWORD dwMode = 0;
        if (!GetConsoleMode(hOut, &dwMode)) return false;
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        return SetConsoleMode(hOut, dwMode) != 0;
    }

    static std::string highlightLine(const std::string& line, HighlightState& state, BatLanguage language, bool plain) {
        if (plain) return line;

        std::string result;
        size_t i = 0;
        while (i < line.size()) {
            if (state.inBlockComment) {
                size_t end = line.find(state.blockTerminator, i);
                if (end == std::string::npos) {
                    result += BatColor::GRAY + line.substr(i) + BatColor::RESET;
                    return result;
                }
                result += BatColor::GRAY + line.substr(i, end + state.blockTerminator.size() - i) + BatColor::RESET;
                i = end + state.blockTerminator.size();
                state.inBlockComment = false;
                state.blockTerminator.clear();
                continue;
            }
            if (state.stringDelimiter != 0) {
                const char quote = state.stringDelimiter;
                const size_t start = i;
                while (i < line.size()) {
                    if (line[i] == '\\' && i + 1 < line.size()) i += 2;
                    else if (line[i++] == quote) { state.stringDelimiter = 0; break; }
                }
                result += BatColor::GREEN + line.substr(start, i - start) + BatColor::RESET;
                if (state.stringDelimiter != 0) return result;
                continue;
            }
            if ((language == BatLanguage::Html || language == BatLanguage::Xml) && line.compare(i, 4, "<!--") == 0) {
                size_t end = line.find("-->", i + 4);
                if (end == std::string::npos) {
                    result += BatColor::GRAY + line.substr(i) + BatColor::RESET;
                    state.inBlockComment = true;
                    state.blockTerminator = "-->";
                    return result;
                }
                result += BatColor::GRAY + line.substr(i, end + 3 - i) + BatColor::RESET;
                i = end + 3;
                continue;
            }
            if ((language == BatLanguage::Pascal) && line.compare(i, 2, "(*") == 0) {
                state.inBlockComment = true;
                state.blockTerminator = "*)";
                continue;
            }
            if ((language == BatLanguage::Pascal) && line[i] == '{') {
                state.inBlockComment = true;
                state.blockTerminator = "}";
                continue;
            }
            if ((language == BatLanguage::Cpp || language == BatLanguage::Go || language == BatLanguage::Java || language == BatLanguage::TypeScript || language == BatLanguage::Pascal) && line.compare(i, 2, "//") == 0) {
                result += BatColor::GRAY + line.substr(i) + BatColor::RESET;
                break;
            }
            if ((language == BatLanguage::Python || language == BatLanguage::Shell || language == BatLanguage::Basic) && line[i] == '#') {
                result += BatColor::GRAY + line.substr(i) + BatColor::RESET;
                break;
            }
            if ((language == BatLanguage::Cpp || language == BatLanguage::Go || language == BatLanguage::Java || language == BatLanguage::TypeScript) && line.compare(i, 2, "/*") == 0) {
                state.inBlockComment = true;
                state.blockTerminator = "*/";
                continue;
            }
            if (language == BatLanguage::Cobol && ((line.size() >= 7 && line[6] == '*') || line.compare(i, 2, "*>") == 0)) {
                result += BatColor::GRAY + line.substr(i) + BatColor::RESET;
                break;
            }
            if (language == BatLanguage::Cpp && i == 0 && line[i] == '#') {
                result += BatColor::MAGENTA + line.substr(i) + BatColor::RESET;
                break;
            }
            if ((language == BatLanguage::Html || language == BatLanguage::Xml) && line[i] == '<') {
                size_t end = line.find('>', i + 1);
                if (end != std::string::npos) {
                    result += BatColor::B_CYAN + line.substr(i, end + 1 - i) + BatColor::RESET;
                    i = end + 1;
                    continue;
                }
            }
            if (line[i] == '"' || line[i] == '\'' || ((language == BatLanguage::Go || language == BatLanguage::TypeScript) && line[i] == '`')) {
                char quote = line[i];
                result += BatColor::GREEN;
                result += quote;
                ++i;
                while (i < line.size()) {
                    result += line[i];
                    if (line[i] == '\\' && i + 1 < line.size()) {
                        result += line[++i];
                    } else if (line[i] == quote) {
                        break;
                    }
                    ++i;
                }
                result += BatColor::RESET;
                if (i < line.size()) ++i;
                continue;
            }
            if (std::isalpha(static_cast<unsigned char>(line[i])) || line[i] == '_') {
                size_t start = i;
                while (i < line.size() && (std::isalnum(static_cast<unsigned char>(line[i])) || line[i] == '_')) {
                    ++i;
                }
                std::string word = line.substr(start, i - start);
                if (inSet(word, language)) {
                    result += BatColor::CYAN + word + BatColor::RESET;
                } else {
                    result += word;
                }
                continue;
            }
            if (std::isdigit(static_cast<unsigned char>(line[i]))) {
                result += BatColor::B_YELLOW;
                while (i < line.size() && (std::isdigit(static_cast<unsigned char>(line[i])) || line[i] == '.' || line[i] == 'x' || line[i] == 'X')) {
                    result += line[i++];
                }
                result += BatColor::RESET;
                continue;
            }
            result += line[i++];
        }
        return result;
    }
};

class BatEngine {
private:
    BatOptions options;

    void processStream(std::istream& in, const std::string& label, std::ostream& out) const {
        const BatLanguage language = SyntaxHighlighter::detectLanguage(label, options.forcedLanguage);
        if (options.showHeader && !options.plain) {
            out << BatColor::GRAY << "───────┬─────────────────────────────────────────\n"
                << "       │ File: " << label << "\n"
                << "───────┼─────────────────────────────────────────\n" << BatColor::RESET;
        }

        HighlightState state;
        std::string line;
        int lineNum = 1;

        while (std::getline(in, line)) {
            if (options.showLineNumbers && !options.plain) {
                out << BatColor::GRAY << std::setw(6) << lineNum++ << " │ " << BatColor::RESET;
            } else if (options.showLineNumbers) {
                out << std::setw(6) << lineNum++ << "  ";
            }
            out << SyntaxHighlighter::highlightLine(line, state, language, options.plain) << "\n";
        }

        if (options.showHeader && !options.plain) {
            out << BatColor::GRAY << "───────┴─────────────────────────────────────────\n" << BatColor::RESET;
        }
    }

public:
    explicit BatEngine(BatOptions opts) : options(std::move(opts)) {}

    int execute() {
        SyntaxHighlighter::initConsole();

        std::ostringstream captured;
        std::ostream* outStream = (options.outputFormat || !options.pipeCommand.empty()) ? &captured : &std::cout;

        for (const auto& file : options.files) {
            if (file == "-") {
                processStream(std::cin, "standard input", *outStream);
            } else {
                std::ifstream in(file);
                if (!in.is_open()) {
                    std::cerr << "bat: cannot open '" << file << "'\n";
                    continue;
                }
                processStream(in, file, *outStream);
            }
        }

        if (options.outputFormat || !options.pipeCommand.empty()) {
            BatReporter::dispatch(captured.str(), options.outputFormat, options.pipeCommand);
        }

        return 0;
    }
};

// ============================================================================
// 4. APPLICATION CONTROLLER
// ============================================================================

class BatApp {
public:
    static int run(int argc, char* argv[]) {
        BatOptions options;
        if (!BatOptions::parse(argc, argv, options)) {
            return 1;
        }
        BatEngine engine(std::move(options));
        return engine.execute();
    }
};

int main(int argc, char* argv[]) {
    return BatApp::run(argc, argv);
}