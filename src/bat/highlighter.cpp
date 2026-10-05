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
 *
 * CrossShell for UNIX
 */

#include "highlighter.hpp"
#include "color.hpp"
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <unordered_set>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace fs = std::filesystem;

std::string SyntaxHighlighter::lower(std::string value) {
    for (char& ch : value) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}

bool SyntaxHighlighter::inSet(const std::string& word, BatLanguage language) {
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

BatLanguage SyntaxHighlighter::detectLanguage(const std::string& label, const std::string& forced) {
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

bool SyntaxHighlighter::initConsole() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return false;
    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode)) return false;
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(hOut, dwMode) != 0;
#else
    return true;
#endif
}

std::string SyntaxHighlighter::highlightLine(const std::string& line, HighlightState& state, BatLanguage language, bool plain) {
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
