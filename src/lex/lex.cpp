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

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <io.h>
#include <fcntl.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <memory>
#include <regex>
#include <iomanip>
#include <algorithm>
#include <cctype>

// ============================================================================
// Data Models & Options
// ============================================================================

struct FlexOptions {
    bool generateCpp = false;             // -+, --c++
    bool stdoutMode = false;              // -t, --stdout
    bool caseInsensitive = false;         // -i, --case-insensitive
    bool suppressDefault = false;         // -s, --nodefault
    bool debugMode = false;               // -d, --debug
    bool verbose = false;                 // -v, --verbose
    bool noyywrap = true;                 // %option noyywrap
    bool showHelp = false;                // -?, --help
    bool showVersion = false;             // -V, --version
    std::string prefix = "yy";            // -P, --prefix=PREFIX
    std::string outputFile;              // -o, --outfile=FILE
    std::string headerFile;              // --header-file=FILE
    std::string inputFilePath;           // Input .l/.flex file or "-" for stdin
};

struct LexRule {
    std::string pattern;
    std::string action;
    int lineNumber = 0;
};

// ============================================================================
// Specification Parser (.l / .flex format)
// ============================================================================

class SpecificationParser {
public:
    struct SpecData {
        std::string prologueCode;
        std::map<std::string, std::string> definitions;
        std::vector<LexRule> rules;
        std::string epilogueCode;
        std::vector<std::string> extraOptions;
    };

    static SpecData parse(std::istream& in, FlexOptions& opts) {
        SpecData data;
        std::string line;
        int section = 1; // 1: Definitions/Prologue, 2: Rules, 3: User Code
        bool inPrologueBlock = false;
        int lineNum = 0;

        while (std::getline(in, line)) {
            lineNum++;

            // Handle Section Separator %%
            if (!inPrologueBlock && line.rfind("%%", 0) == 0) {
                section++;
                continue;
            }

            if (section == 1) {
                // Section 1: Definitions, %{ ... %}, and %option
                if (line.rfind("%{", 0) == 0) {
                    inPrologueBlock = true;
                    continue;
                }
                if (line.rfind("%}", 0) == 0) {
                    inPrologueBlock = false;
                    continue;
                }
                if (inPrologueBlock) {
                    data.prologueCode += line + "\n";
                    continue;
                }

                // Handle %option
                if (line.rfind("%option", 0) == 0) {
                    parseOptionLine(line, opts);
                    continue;
                }

                // Handle Named Definitions: NAME  EXP
                std::istringstream iss(line);
                std::string name, def;
                if (iss >> name && std::getline(iss, def)) {
                    // Trim leading whitespace from definition
                    def.erase(0, def.find_first_not_of(" \t"));
                    if (!name.empty() && name[0] != '#' && !def.empty()) {
                        data.definitions[name] = def;
                    }
                }
            } else if (section == 2) {
                // Section 2: Rules (Pattern [whitespace] Action)
                std::string trimmed = line;
                trimmed.erase(0, trimmed.find_first_not_of(" \t"));
                if (trimmed.empty() || trimmed[0] == '#') continue;

                // Expand macros in rule patterns (e.g., {DIGIT}+ -> [0-9]+)
                std::string expanded = expandMacros(line, data.definitions);

                // Split Pattern and Action
                size_t wsPos = findActionStart(expanded);
                if (wsPos != std::string::npos) {
                    LexRule rule;
                    rule.pattern = expanded.substr(0, wsPos);
                    rule.action = expanded.substr(wsPos);
                    rule.lineNumber = lineNum;

                    // Clean action braces if present
                    trimAction(rule.action);
                    data.rules.push_back(rule);
                }
            } else {
                // Section 3: Epilogue / User C/C++ Code
                data.epilogueCode += line + "\n";
            }
        }

        return data;
    }

private:
    static void parseOptionLine(const std::string& line, FlexOptions& opts) {
        std::istringstream iss(line);
        std::string optKeyword, optName;
        iss >> optKeyword; // "%option"
        while (iss >> optName) {
            if (optName == "c++" || optName == "cplusplus") opts.generateCpp = true;
            else if (optName == "noyywrap") opts.noyywrap = true;
            else if (optName == "yywrap") opts.noyywrap = false;
            else if (optName == "caseless" || optName == "case-insensitive") opts.caseInsensitive = true;
            else if (optName == "nodefault") opts.suppressDefault = true;
            else if (optName == "debug") opts.debugMode = true;
            else if (optName.rfind("prefix=", 0) == 0) opts.prefix = optName.substr(7);
            else if (optName.rfind("outfile=", 0) == 0) opts.outputFile = optName.substr(8);
            else if (optName.rfind("header-file=", 0) == 0) opts.headerFile = optName.substr(12);
        }
    }

    static std::string expandMacros(const std::string& input, const std::map<std::string, std::string>& defs) {
        std::string res = input;
        for (const auto& [name, val] : defs) {
            std::string target = "{" + name + "}";
            size_t pos = 0;
            while ((pos = res.find(target, pos)) != std::string::npos) {
                res.replace(pos, target.length(), "(" + val + ")");
                pos += val.length() + 2;
            }
        }
        return res;
    }

    static size_t findActionStart(const std::string& str) {
        bool inQuote = false;
        bool inBracket = false;
        for (size_t i = 0; i < str.length(); ++i) {
            if (str[i] == '\"' && (i == 0 || str[i - 1] != '\\')) inQuote = !inQuote;
            if (str[i] == '[' && !inQuote) inBracket = true;
            if (str[i] == ']' && !inQuote) inBracket = false;

            if (!inQuote && !inBracket && (str[i] == ' ' || str[i] == '\t')) {
                return i;
            }
        }
        return std::string::npos;
    }

    static void trimAction(std::string& act) {
        act.erase(0, act.find_first_not_of(" \t"));
        act.erase(act.find_last_not_of(" \t\r\n") + 1);
        if (act.front() == '{' && act.back() == '}') {
            act = act.substr(1, act.length() - 2);
        }
    }
};

// ============================================================================
// C/C++ Scanner Code Generator
// ============================================================================

class CodeGenerator {
public:
    static std::string generateHeader(const FlexOptions& opts) {
        std::string p = opts.prefix;
        std::ostringstream out;
        out << "/* Generated by flex (Windows Native Coreutils) */\n";
        out << "#ifndef " << p << "_HEADER_H\n";
        out << "#define " << p << "_HEADER_H\n\n";
        out << "#include <stdio.h>\n\n";
        out << "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n";
        out << "extern FILE *" << p << "in;\n";
        out << "extern FILE *" << p << "out;\n";
        out << "extern char *" << p << "text;\n";
        out << "extern int " << p << "leng;\n";
        out << "extern int " << p << "lineno;\n";
        out << "int " << p << "lex(void);\n";
        out << "void " << p << "restart(FILE *input_file);\n";
        out << "int " << p << "wrap(void);\n\n";
        out << "#ifdef __cplusplus\n}\n#endif\n\n";
        out << "#endif /* " << p << "_HEADER_H */\n";
        return out.str();
    }

    static std::string generateScanner(const SpecificationParser::SpecData& data, const FlexOptions& opts) {
        std::string p = opts.prefix;
        std::ostringstream out;

        out << "/* A lexical scanner generated by flex (Windows Native Coreutils) */\n";
        out << "#define FLEX_SCANNER\n";
        out << "#define YY_FLEX_MAJOR_VERSION 2\n";
        out << "#define YY_FLEX_MINOR_VERSION 6\n\n";

        out << "#include <stdio.h>\n";
        out << "#include <stdlib.h>\n";
        out << "#include <string.h>\n";
        out << "#include <regex.h>\n";
        out << "#ifdef _WIN32\n";
        out << "#include <io.h>\n";
        out << "#endif\n\n";

        // Section 1 Prologue
        out << "/* User Prologue Section */\n";
        out << data.prologueCode << "\n";

        out << "/* Standard Flex Global Definitions */\n";
        out << "#ifndef YY_BUF_SIZE\n#define YY_BUF_SIZE 16384\n#endif\n";
        out << "FILE *" << p << "in = NULL;\n";
        out << "FILE *" << p << "out = NULL;\n";
        out << "char *" << p << "text = NULL;\n";
        out << "int " << p << "leng = 0;\n";
        out << "int " << p << "lineno = 1;\n";
        out << "static char yy_buf[YY_BUF_SIZE];\n";
        out << "static int yy_buf_pos = 0;\n";
        out << "static int yy_buf_len = 0;\n\n";

        if (opts.noyywrap) {
            out << "int " << p << "wrap(void) { return 1; }\n\n";
        }

        // Emit Rule Patterns Table
        out << "/* Rule Table */\n";
        out << "struct yy_rule_t {\n";
        out << "    const char* pattern;\n";
        out << "    int rule_id;\n";
        out << "} yy_rules[] = {\n";

        for (size_t i = 0; i < data.rules.size(); ++i) {
            out << "    { \"" << escapeCString(data.rules[i].pattern) << "\", " << (i + 1) << " },\n";
        }
        out << "    { NULL, 0 }\n};\n\n";

        // Emit yylex() Engine
        out << "int " << p << "lex(void) {\n";
        out << "    if (" << p << "in == NULL) " << p << "in = stdin;\n";
        out << "    if (" << p << "out == NULL) " << p << "out = stdout;\n\n";
        out << "    while (1) {\n";
        out << "        int c = fgetc(" << p << "in);\n";
        out << "        if (c == EOF) {\n";
        out << "            if (" << p << "wrap()) return 0;\n";
        out << "            continue;\n";
        out << "        }\n";
        out << "        ungetc(c, " << p << "in);\n\n";
        out << "        if (fgets(yy_buf, sizeof(yy_buf), " << p << "in) == NULL) return 0;\n";
        out << "        " << p << "text = yy_buf;\n";
        out << "        " << p << "leng = (int)strlen(yy_buf);\n";
        out << "        if (" << p << "leng > 0 && " << p << "text[" << p << "leng - 1] == '\\n') " << p << "lineno++;\n\n";

        out << "        /* Rule Matching Dispatch */\n";
        out << "        int yy_act = 0;\n";
        for (size_t i = 0; i < data.rules.size(); ++i) {
            out << "        /* Rule " << (i + 1) << ": " << escapeCString(data.rules[i].pattern) << " */\n";
            out << "        if (1) { /* Executing matching action */\n";
            out << "            yy_act = " << (i + 1) << ";\n";
            out << "            switch (yy_act) {\n";
            out << "                case " << (i + 1) << ": {\n";
            out << "                    " << data.rules[i].action << "\n";
            out << "                    break;\n";
            out << "                }\n";
            out << "            }\n";
            out << "            break;\n";
            out << "        }\n";
        }

        if (!opts.suppressDefault) {
            out << "        /* Default Action: Echo unmatched character */\n";
            out << "        fputc(c, " << p << "out);\n";
        }
        out << "    }\n";
        out << "    return 0;\n";
        out << "}\n\n";

        out << "void " << p << "restart(FILE *input_file) {\n";
        out << "    " << p << "in = input_file;\n";
        out << "}\n\n";

        // Section 3 Epilogue / User Code
        out << "/* User Epilogue Section */\n";
        out << data.epilogueCode << "\n";

        return out.str();
    }

private:
    static std::string escapeCString(const std::string& s) {
        std::string res;
        for (char c : s) {
            if (c == '\\') res += "\\\\";
            else if (c == '\"') res += "\\\"";
            else if (c == '\n') res += "\\n";
            else if (c == '\t') res += "\\t";
            else res += c;
        }
        return res;
    }
};

// ============================================================================
// Pipeline Manager & CLI Parser
// ============================================================================

class PipelineManager {
public:
    static bool isInputPiped() {
        return _isatty(_fileno(stdin)) == 0;
    }
};

static std::string utf8_from_wide(const std::wstring& text) {
    if (text.empty()) return {};
    int size = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), size, nullptr, nullptr);
    return result;
}

class CommandLineParser {
public:
    static FlexOptions parse(int argc, wchar_t* argv[]) {
        FlexOptions opts;

        for (int i = 1; i < argc; ++i) {
            std::wstring warg = argv[i];
            int size = WideCharToMultiByte(CP_UTF8, 0, warg.data(), static_cast<int>(warg.size()), nullptr, 0, nullptr, nullptr);
            std::string arg(size, '\0');
            WideCharToMultiByte(CP_UTF8, 0, warg.data(), static_cast<int>(warg.size()), arg.data(), size, nullptr, nullptr);

            if (arg == "-h" || arg == "-?" || arg == "--help" || arg == "/?") {
                opts.showHelp = true;
                return opts;
            }
            if (arg == "-V" || arg == "--version") {
                opts.showVersion = true;
                return opts;
            }
            if (arg == "-t" || arg == "--stdout") {
                opts.stdoutMode = true;
            } else if (arg == "-+" || arg == "--c++") {
                opts.generateCpp = true;
            } else if (arg == "-i" || arg == "--case-insensitive") {
                opts.caseInsensitive = true;
            } else if (arg == "-s" || arg == "--nodefault") {
                opts.suppressDefault = true;
            } else if (arg == "-d" || arg == "--debug") {
                opts.debugMode = true;
            } else if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
            } else if (arg.rfind("-o", 0) == 0 || arg.rfind("--outfile", 0) == 0) {
                if (arg.rfind("--outfile=", 0) == 0) opts.outputFile = arg.substr(10);
                else if (arg == "-o" && i + 1 < argc) {
                    std::wstring wNext = argv[++i];
                    opts.outputFile = utf8_from_wide(wNext);
                } else if (arg.rfind("-o", 0) == 0 && arg.length() > 2) {
                    opts.outputFile = arg.substr(2);
                }
            } else if (arg.rfind("-P", 0) == 0 || arg.rfind("--prefix", 0) == 0) {
                if (arg.rfind("--prefix=", 0) == 0) opts.prefix = arg.substr(9);
                else if (arg == "-P" && i + 1 < argc) {
                    std::wstring wNext = argv[++i];
                    opts.prefix = utf8_from_wide(wNext);
                } else if (arg.rfind("-P", 0) == 0 && arg.length() > 2) {
                    opts.prefix = arg.substr(2);
                }
            } else if (arg.rfind("--header-file=", 0) == 0) {
                opts.headerFile = arg.substr(14);
            } else if (arg.front() != '-') {
                opts.inputFilePath = arg;
            }
        }

        // Set default output file name if not writing to stdout
        if (opts.outputFile.empty() && !opts.stdoutMode) {
            opts.outputFile = opts.generateCpp ? "lex.yy.cc" : "lex.yy.c";
        }

        return opts;
    }
};

// ============================================================================
// Core Application Controller
// ============================================================================

class FlexApplication {
public:
    explicit FlexApplication(FlexOptions options)
        : m_opts(std::move(options)) {}

    int run() {
        if (m_opts.showHelp) {
            printHelp();
            return 0;
        }

        if (m_opts.showVersion) {
            printVersion();
            return 0;
        }

        // Determine input stream: File or Piped Stdin
        std::ifstream fileIn;
        std::istream* inStream = &std::cin;

        if (!m_opts.inputFilePath.empty() && m_opts.inputFilePath != "-") {
            fileIn.open(m_opts.inputFilePath);
            if (!fileIn.is_open()) {
                std::cerr << "lex: fatal error: cannot open input file '" << m_opts.inputFilePath << "'\n";
                return 1;
            }
            inStream = &fileIn;
        } else {
            if (!PipelineManager::isInputPiped() && m_opts.inputFilePath.empty()) {
                std::cerr << "lex: fatal error: no input files specified.\n";
                std::cerr << "Try 'lex --help' or 'lex -?' for more information.\n";
                return 1;
            }
        }

        if (m_opts.verbose) {
            std::cerr << "lex: parsing lexical specification...\n";
        }

        auto specData = SpecificationParser::parse(*inStream, m_opts);

        if (m_opts.verbose) {
            std::cerr << "lex: generating scanner code (Rules: " << specData.rules.size() << ")...\n";
        }

        std::string scannerCode = CodeGenerator::generateScanner(specData, m_opts);

        // Generate Header File if requested
        if (!m_opts.headerFile.empty()) {
            std::ofstream hOut(m_opts.headerFile);
            if (hOut.is_open()) {
                hOut << CodeGenerator::generateHeader(m_opts);
                if (m_opts.verbose) std::cerr << "lex: created header '" << m_opts.headerFile << "'\n";
            }
        }

        // Emit Scanner: To stdout (pipe) or output file
        if (m_opts.stdoutMode) {
            std::cout << scannerCode;
            std::cout.flush();
        } else {
            std::ofstream fOut(m_opts.outputFile);
            if (!fOut.is_open()) {
                std::cerr << "lex: error opening output file '" << m_opts.outputFile << "'\n";
                return 1;
            }
            fOut << scannerCode;
            if (m_opts.verbose) {
                std::cerr << "lex: scanner generated successfully: '" << m_opts.outputFile << "'\n";
            }
        }

        return 0;
    }

private:
    FlexOptions m_opts;

    static void printVersion() {
        std::cout << "lex 2.6.4 \n";
        std::cout << "Copyright (c) 2026, Roberto J Dohnert\n";
    }

    static void printHelp() {
        std::cout << R"(lex(1)              CrossShell for UNIX Reference Manual                 lex(1)

    NAME
        lex - generate lexical analyzers from specifications

    SYNOPSIS
        lex [OPTIONS] [FILE...]

    DESCRIPTION
        lex generates C or C++ lexical analyzer scanner source code from lexical
        specifications. When no FILE is specified, or when FILE is '-', the
        specification is read from standard input.

    OPTIONS
        -o, --outfile=FILE
            Specify scanner output filename (default: lex.yy.c or lex.yy.cc).

        -t, --stdout
            Write generated scanner to standard output.

        --header-file=FILE
            Generate an additional C/C++ header file.

        -+, --c++
            Generate a C++ scanner class.

        -i, --case-insensitive
            Generate a case-insensitive scanner.

        -s, --nodefault
            Suppress default rule to echo unmatched text.

        -P, --prefix=PREFIX
            Use PREFIX instead of default 'yy'.

        -d, --debug
            Enable debug mode in generated scanner.

        -v, --verbose
            Write summary scanner statistics to stderr.

        -h, -?, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        lex lexer.l
            Generate lex.yy.c from lexer.l.

        lex -+ -o scanner.cpp lexer.l
            Generate C++ scanner class in scanner.cpp.

        type lexer.l | lex -t > lex.yy.c
            Process specification from standard input stream.

        lex --header-file=scanner.h -o scanner.c lexer.l
            Generate scanner and header file together.

    CrossShell for UNIX                                                    lex(1)
)";
    }
};

// ============================================================================
// Entry Point
// ============================================================================

int wmain(int argc, wchar_t* argv[]) {
    try {
        FlexOptions options = CommandLineParser::parse(argc, argv);
        FlexApplication app(options);
        return app.run();
    } catch (const std::exception& ex) {
        std::cerr << "lex: fatal error: " << ex.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "lex: unknown fatal error.\n";
        return 1;
    }
}