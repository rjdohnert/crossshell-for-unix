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

#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iostream>
#include <vector>
#include <deque>
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <unordered_map>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <cctype>
#include <thread>
#include <atomic>
#include <mutex>

#pragma comment(lib, "User32.lib")

namespace fs = std::filesystem;

static std::string ReadEnvironmentVariableString(const char* name) {
    if (!name || *name == '\0') {
        return "";
    }

    DWORD needed = GetEnvironmentVariableA(name, NULL, 0);
    if (needed == 0) {
        return "";
    }

    std::vector<char> buffer(static_cast<size_t>(needed));
    DWORD written = GetEnvironmentVariableA(name, buffer.data(), needed);
    if (written == 0 || written >= needed) {
        return "";
    }

    return std::string(buffer.data(), written);
}

// Converts a UTF-8 / ANSI narrow string to a wide string for Win32 wide APIs.
static std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (needed <= 0) return L"";
    std::wstring out(static_cast<size_t>(needed), L'\0');
    int written = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), out.data(), needed);
    if (written <= 0) return L"";
    out.resize(static_cast<size_t>(written));
    return out;
}

// ============================================================================
// CONSTANTS & ENUMS
// ============================================================================

const std::string APP_TITLE = "CrossForge IDE v3.0.2";
const std::string COPYRIGHT_NOTICE = "Copyright (C) 2026, Roberto J. Dohnert";

enum class SupportedLanguage {
    Cpp, C, Java, CSharp, ObjC, Swift, TypeScript, JavaScript, NodeJS, Pascal, Fortran, Cobol, Rust, Go, Text
};

enum ThemeScheme {
    Theme_BorlandBlue = 0,
    Theme_Monokai,
    Theme_Matrix,
    Theme_Cyberpunk,
    Theme_RetroAmber
};

struct ThemePalette {
    WORD MenuAttr;
    WORD MenuHotkeyAttr;
    WORD DesktopAttr;
    WORD WinBorderActive;
    WORD WinBorderInactive;
    WORD WinTitleAttr;
    WORD EditorBg;
    WORD TextDefault;
    WORD KeywordAttr;
    WORD StringAttr;
    WORD CommentAttr;
    WORD NumberAttr;
    WORD LineNumAttr;
    WORD StatusBarAttr;
};

// ============================================================================
// THEME SYSTEM
// ============================================================================

class ThemeManager {
public:
    static ThemePalette GetTheme(ThemeScheme scheme) {
        ThemePalette p{};
        switch (scheme) {
        case Theme_Monokai:
            p.MenuAttr          = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
            p.MenuHotkeyAttr    = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY;
            p.DesktopAttr       = 0;
            p.WinBorderActive   = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.WinBorderInactive = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            p.WinTitleAttr      = BACKGROUND_RED | BACKGROUND_GREEN | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            p.EditorBg          = 0;
            p.TextDefault       = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            p.KeywordAttr       = FOREGROUND_RED | FOREGROUND_INTENSITY;
            p.StringAttr        = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.CommentAttr       = FOREGROUND_GREEN;
            p.NumberAttr        = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.LineNumAttr       = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
            p.StatusBarAttr     = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
            break;

        case Theme_Matrix:
            p.MenuAttr          = BACKGROUND_GREEN;
            p.MenuHotkeyAttr    = BACKGROUND_GREEN | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            p.DesktopAttr       = 0;
            p.WinBorderActive   = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.WinBorderInactive = FOREGROUND_GREEN;
            p.WinTitleAttr      = BACKGROUND_GREEN | FOREGROUND_INTENSITY;
            p.EditorBg          = 0;
            p.TextDefault       = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.KeywordAttr       = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
            p.StringAttr        = FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY;
            p.CommentAttr       = FOREGROUND_GREEN;
            p.NumberAttr        = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.LineNumAttr       = FOREGROUND_GREEN;
            p.StatusBarAttr     = BACKGROUND_GREEN;
            break;

        case Theme_BorlandBlue:
        default:
            // Default palette with blue editor background.
            p.MenuAttr          = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY;
            p.MenuHotkeyAttr    = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_INTENSITY;
            p.DesktopAttr       = BACKGROUND_BLUE;
            p.WinBorderActive   = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.WinBorderInactive = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.WinTitleAttr      = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.EditorBg          = BACKGROUND_BLUE;
            p.TextDefault       = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.KeywordAttr       = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.StringAttr        = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.CommentAttr       = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.NumberAttr        = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.LineNumAttr       = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
            p.StatusBarAttr     = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
            break;
        }
        return p;
    }
};

// ============================================================================
// LANGUAGE REGISTRY & SYNTAX HIGHLIGHTER
// ============================================================================

struct LanguageConfig {
    SupportedLanguage Lang;
    std::string Name;
    std::vector<std::string> Extensions;
    std::vector<std::string> Keywords;
    std::string CompilerExe;
    std::string DebuggerExe;
    std::string CompileCmdTemplate;
    std::string DebugCmdTemplate;
};

class LanguageRegistry {
private:
    std::map<SupportedLanguage, LanguageConfig> m_configs;

public:
    LanguageRegistry() {
        InitLanguages();
        const char* modernCppKeywords[] = {
            "alignof", "char8_t", "char16_t", "char32_t", "nullptr", "static_assert",
            "static_cast", "dynamic_cast", "const_cast", "reinterpret_cast", "thread_local",
            "concept", "requires", "co_await", "co_return", "co_yield"
        };
        for (const char* keyword : modernCppKeywords) {
            m_configs[SupportedLanguage::Cpp].Keywords.push_back(keyword);
        }
    }

    void InitLanguages() {
        // 1. C++
        m_configs[SupportedLanguage::Cpp] = {
            SupportedLanguage::Cpp, "C++", {".cpp", ".cxx", ".cc", ".h", ".hpp"},
            {"alignas", "auto", "bool", "break", "case", "catch", "class", "const", "constexpr",
             "continue", "default", "delete", "do", "double", "else", "enum", "explicit", "export",
             "extern", "false", "float", "for", "friend", "goto", "if", "inline", "int", "long",
             "namespace", "new", "operator", "private", "protected", "public", "return", "short",
             "signed", "sizeof", "static", "struct", "switch", "template", "this", "throw", "true",
             "try", "typedef", "typename", "union", "unsigned", "using", "virtual", "void", "while"},
            "cl.exe", "cdb.exe", "cl.exe /std:c++17 /EHsc \"%f\"", "cdb.exe -g \"%e\""
        };

        // 2. C
        m_configs[SupportedLanguage::C] = {
            SupportedLanguage::C, "C", {".c", ".h"},
            {"auto", "break", "case", "char", "const", "continue", "default", "do", "double", "else",
             "enum", "extern", "float", "for", "goto", "if", "int", "long", "register", "return",
             "short", "signed", "sizeof", "static", "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while"},
            "gcc.exe", "gdb.exe", "gcc \"%f\" -o \"%e.exe\"", "gdb \"%e.exe\""
        };

        // 3. Java
        m_configs[SupportedLanguage::Java] = {
            SupportedLanguage::Java, "Java", {".java"},
            {"abstract", "boolean", "break", "byte", "case", "catch", "char", "class", "continue",
             "default", "do", "double", "else", "extends", "final", "finally", "float", "for", "if",
             "implements", "import", "instanceof", "int", "interface", "long", "native", "new", "package",
             "private", "protected", "public", "return", "short", "static", "super", "switch", "synchronized",
             "this", "throw", "throws", "transient", "try", "void", "volatile", "while"},
            "javac.exe", "jdb.exe", "javac \"%f\"", "jdb \"%e\""
        };

        // 4. C#
        m_configs[SupportedLanguage::CSharp] = {
            SupportedLanguage::CSharp, "C#", {".cs"},
            {"abstract", "as", "base", "bool", "break", "byte", "case", "catch", "char", "checked",
             "class", "const", "continue", "decimal", "default", "delegate", "do", "double", "else",
             "enum", "event", "explicit", "extern", "false", "finally", "fixed", "float", "for",
             "foreach", "goto", "if", "implicit", "in", "int", "interface", "internal", "is", "lock",
             "long", "namespace", "new", "null", "object", "operator", "out", "override", "params",
             "private", "protected", "public", "readonly", "ref", "return", "sbyte", "sealed", "short",
             "sizeof", "stackalloc", "static", "string", "struct", "switch", "this", "throw", "true",
             "try", "typeof", "uint", "ulong", "unchecked", "unsafe", "ushort", "using", "virtual", "void", "while"},
            "csc.exe", "vsdbg.exe", "csc /target:exe \"%f\"", "vsdbg --engine \"%e.exe\""
        };

        // 5. Objective-C
        m_configs[SupportedLanguage::ObjC] = {
            SupportedLanguage::ObjC, "Objective-C", {".m", ".mm"},
            {"@interface", "@implementation", "@end", "@property", "@synthesize", "@selector", "@protocol",
             "id", "Class", "SEL", "BOOL", "YES", "NO", "nil", "Nil", "self", "super", "alloc", "init"},
            "clang.exe", "lldb.exe", "clang -framework Foundation \"%f\" -o \"%e.exe\"", "lldb \"%e.exe\""
        };

        // 6. Swift
        m_configs[SupportedLanguage::Swift] = {
            SupportedLanguage::Swift, "Swift", {".swift"},
            {"class", "deinit", "enum", "extension", "func", "import", "init", "inout", "internal", "let",
             "operator", "private", "protocol", "public", "static", "struct", "subscript", "typealias", "var",
             "break", "case", "continue", "default", "defer", "do", "else", "fallthrough", "for", "guard", "if",
             "in", "repeat", "return", "switch", "where", "while", "as", "catch", "false", "is", "nil", "rethrows",
             "super", "self", "Self", "throw", "true", "try"},
            "swiftc.exe", "lldb.exe", "swiftc \"%f\" -o \"%e.exe\"", "lldb \"%e.exe\""
        };

        // 7. TypeScript
        m_configs[SupportedLanguage::TypeScript] = {
            SupportedLanguage::TypeScript, "TypeScript", {".ts", ".tsx"},
            {"any", "boolean", "constructor", "declare", "get", "instanceof", "interface", "enum", "type",
             "number", "private", "protected", "public", "readonly", "require", "set", "string", "symbol",
             "module", "namespace", "abstract", "as", "implements", "is", "let", "const", "var", "function", "import", "export"},
            "tsc.cmd", "node.exe", "tsc \"%f\"", "node --inspect-brk \"%e.js\""
        };

        // 8. JavaScript
        m_configs[SupportedLanguage::JavaScript] = {
            SupportedLanguage::JavaScript, "JavaScript", {".js", ".jsx"},
            {"async", "await", "break", "case", "catch", "class", "const", "continue", "debugger", "default",
             "delete", "do", "else", "export", "extends", "false", "finally", "for", "function", "if",
             "import", "in", "instanceof", "new", "null", "return", "super", "switch", "this", "throw",
             "true", "try", "typeof", "var", "void", "while", "with", "yield", "let"},
            "node.exe", "node.exe", "node \"%f\"", "node --inspect-brk \"%f\""
        };

        // 9. Node.js
        m_configs[SupportedLanguage::NodeJS] = {
            SupportedLanguage::NodeJS, "Node.js", {".cjs", ".mjs"},
            {"require", "exports", "module", "__dirname", "__filename", "process", "Buffer", "global", "async", "await"},
            "node.exe", "node.exe", "node \"%f\"", "node --inspect-brk \"%f\""
        };

        // 10. Pascal
        m_configs[SupportedLanguage::Pascal] = {
            SupportedLanguage::Pascal, "Pascal", {".pas", ".pp", ".inc"},
            {"absolute", "and", "array", "begin", "case", "const", "constructor", "destructor", "div",
             "do", "downto", "else", "end", "file", "for", "function", "goto", "if", "implementation",
             "in", "inherited", "inline", "interface", "label", "mod", "nil", "not", "object", "of", "or",
             "packed", "procedure", "program", "record", "repeat", "set", "string", "then", "to", "type",
             "unit", "until", "uses", "var", "while", "with"},
            "fpc.exe", "gdb.exe", "fpc \"%f\"", "gdb \"%e.exe\""
        };

        // 11. Fortran
        m_configs[SupportedLanguage::Fortran] = {
            SupportedLanguage::Fortran, "Fortran", {".f", ".f90", ".f95", ".for"},
            {"program", "end", "subroutine", "function", "integer", "real", "double", "precision", "complex",
             "logical", "character", "dimension", "intent", "implicit", "none", "use", "module", "if", "then",
             "else", "endif", "do", "enddo", "while", "call", "return", "print", "write", "read"},
            "gfortran.exe", "gdb.exe", "gfortran \"%f\" -o \"%e.exe\"", "gdb \"%e.exe\""
        };

        // 12. COBOL
        m_configs[SupportedLanguage::Cobol] = {
            SupportedLanguage::Cobol, "COBOL", {".cob", ".cbl"},
            {"IDENTIFICATION", "DIVISION", "PROGRAM-ID", "ENVIRONMENT", "DATA", "WORKING-STORAGE",
             "PROCEDURE", "DISPLAY", "ACCEPT", "STOP", "RUN", "PERFORM", "UNTIL", "VARYING", "IF", "ELSE",
             "END-IF", "MOVE", "TO", "ADD", "SUBTRACT", "MULTIPLY", "DIVIDE", "COMPUTE", "PIC", "VALUE"},
            "cobc.exe", "gdb.exe", "cobc -x \"%f\" -o \"%e.exe\"", "gdb \"%e.exe\""
        };

        // 13. Rust
        m_configs[SupportedLanguage::Rust] = {
            SupportedLanguage::Rust, "Rust", {".rs"},
            {"as", "async", "await", "break", "const", "continue", "crate", "dyn", "else", "enum",
             "extern", "false", "fn", "for", "if", "impl", "in", "let", "loop", "match", "mod",
             "move", "mut", "pub", "ref", "return", "self", "Self", "static", "struct", "super",
             "trait", "true", "type", "unsafe", "use", "where", "while", "abstract", "become",
             "box", "do", "final", "macro", "override", "priv", "typeof", "unsized", "virtual", "yield"},
            "rustc.exe", "gdb.exe", "rustc \"%f\" -o \"%e.exe\"", "gdb \"%e.exe\""
        };

        // 14. Go
        m_configs[SupportedLanguage::Go] = {
            SupportedLanguage::Go, "Go", {".go"},
            {"break", "default", "func", "interface", "select", "case", "defer", "go", "map", "struct",
             "chan", "else", "goto", "package", "switch", "const", "fallthrough", "if", "range", "type",
             "continue", "for", "import", "return", "var", "true", "false", "nil"},
            "go.exe", "gdb.exe", "go build -o \"%e.exe\" \"%f\"", "gdb \"%e.exe\""
        };
    }

    SupportedLanguage DetectLanguage(const std::string& filepath) {
        std::string ext = fs::path(filepath).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char character) {
            return static_cast<char>(std::tolower(character));
        });

        for (const auto& [lang, cfg] : m_configs) {
            for (const auto& e : cfg.Extensions) {
                if (e == ext) return lang;
            }
        }
        return SupportedLanguage::Text;
    }

    const LanguageConfig& GetConfig(SupportedLanguage lang) const {
        static LanguageConfig defaultConfig{SupportedLanguage::Text, "Plain Text", {}, {}, "", "", "", ""};
        auto it = m_configs.find(lang);
        if (it != m_configs.end()) return it->second;
        return defaultConfig;
    }
};

// ============================================================================
// COMPILER & TOOLCHAIN AUTODETECTION
// ============================================================================

class CompilerDetector {
public:
    struct DetectedTool {
        std::string Name;
        std::string Path;
        bool Found;
    };

    static std::vector<DetectedTool> DetectAll() {
        std::vector<DetectedTool> results;

        // Check Visual Studio Build Tools / MSVC in Program Files
        std::string vsPath = SearchVisualStudio();

        std::vector<std::pair<std::string, std::string>> executables = {
            {"Microsoft Visual C++ (cl.exe)", "cl.exe"},
            {"GCC Compiler (gcc.exe)", "gcc.exe"},
            {"G++ Compiler (g++.exe)", "g++.exe"},
            {"Rust Compiler (rustc.exe)", "rustc.exe"},
            {"Go Toolchain (go.exe)", "go.exe"},
            {"Java Compiler (javac.exe)", "javac.exe"},
            {"C# Compiler (csc.exe)", "csc.exe"},
            {"Free Pascal Compiler (fpc.exe)", "fpc.exe"},
            {"GNU Fortran (gfortran.exe)", "gfortran.exe"},
            {"GnuCOBOL (cobc.exe)", "cobc.exe"},
            {"Node.js Runtime (node.exe)", "node.exe"},
            {"TypeScript Compiler (tsc.cmd)", "tsc.cmd"},
            {"Swift Compiler (swiftc.exe)", "swiftc.exe"}
        };

        for (const auto& tool : executables) {
            std::string foundPath = FindInPath(tool.second);
            if (foundPath.empty() && tool.second == "cl.exe" && !vsPath.empty()) {
                foundPath = vsPath;
            }
            results.push_back({tool.first, foundPath, !foundPath.empty()});
        }

        return results;
    }

    static std::vector<std::string> GetVisualStudioBuildToolDirs() {
        std::vector<std::string> result;
        const char* programFilesEnv[] = {"ProgramFiles", "ProgramFiles(x86)"};
        const std::vector<std::string> years = {"2022", "2019", "2017"};
        const std::vector<std::string> editions = {"Enterprise", "Professional", "Community", "BuildTools"};

        for (const char* envVar : programFilesEnv) {
            char pf[MAX_PATH];
            if (GetEnvironmentVariableA(envVar, pf, MAX_PATH) == 0) {
                continue;
            }

            fs::path base(pf);
            base /= "Microsoft Visual Studio";
            for (const auto& year : years) {
                for (const auto& ed : editions) {
                    fs::path msvcRoot = base / year / ed / "VC/Tools/MSVC";
                    if (!fs::exists(msvcRoot) || !fs::is_directory(msvcRoot)) {
                        continue;
                    }

                    for (const auto& entry : fs::directory_iterator(msvcRoot)) {
                        if (!entry.is_directory()) {
                            continue;
                        }

                        fs::path hostX64x64 = entry.path() / "bin/Hostx64/x64";
                        fs::path hostX64x86 = entry.path() / "bin/Hostx64/x86";
                        fs::path hostX86x86 = entry.path() / "bin/Hostx86/x86";

                        if (fs::exists(hostX64x64 / "cl.exe")) result.push_back(hostX64x64.string());
                        if (fs::exists(hostX64x86 / "cl.exe")) result.push_back(hostX64x86.string());
                        if (fs::exists(hostX86x86 / "cl.exe")) result.push_back(hostX86x86.string());
                    }
                }
            }
        }

        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

    // Locate a Visual Studio developer-environment activation script so builds can
    // set up INCLUDE/LIB/PATH before invoking cl.exe. Prefers VsDevCmd.bat, falls
    // back to vcvars64.bat. Returns "" if none is found.
    static std::string FindVsDevCmdScript() {
        const char* programFilesEnv[] = {"ProgramFiles", "ProgramFiles(x86)"};
        const std::vector<std::string> years = {"2022", "2019", "2017"};
        const std::vector<std::string> editions = {"Enterprise", "Professional", "Community", "BuildTools"};

        for (const char* envVar : programFilesEnv) {
            char pf[MAX_PATH];
            if (GetEnvironmentVariableA(envVar, pf, MAX_PATH) == 0) {
                continue;
            }
            fs::path base = fs::path(pf) / "Microsoft Visual Studio";
            for (const auto& year : years) {
                for (const auto& ed : editions) {
                    fs::path root = base / year / ed;
                    if (!fs::exists(root) || !fs::is_directory(root)) {
                        continue;
                    }
                    fs::path vsdev = root / "Common7/Tools/VsDevCmd.bat";
                    if (fs::exists(vsdev)) return vsdev.string();
                    fs::path vcvars = root / "VC/Auxiliary/Build/vcvars64.bat";
                    if (fs::exists(vcvars)) return vcvars.string();
                }
            }
        }
        return "";
    }

private:
    static std::string FindInPath(const std::string& exeName) {
        std::string pathValue = ReadEnvironmentVariableString("PATH");
        if (pathValue.empty()) return "";

        std::stringstream ss(pathValue);
        std::string item;
        while (std::getline(ss, item, ';')) {
            fs::path p = fs::path(item) / exeName;
            if (fs::exists(p)) {
                return p.string();
            }
        }
        return "";
    }

    static std::string SearchVisualStudio() {
        std::vector<std::string> dirs = GetVisualStudioBuildToolDirs();
        for (const auto& dir : dirs) {
            fs::path clPath = fs::path(dir) / "cl.exe";
            if (fs::exists(clPath)) {
                return clPath.string();
            }
        }
        return "";
    }
};

// ============================================================================
// SANDBOXED SECURITY EXECUTION ENGINE
// ============================================================================

class SandboxedExecutor {
public:
    static DWORD GetProcessTimeoutMs() {
        const DWORD defaultMs = 180000;
        const DWORD minMs = 1000;
        const DWORD maxMs = 600000;

        char envBuf[64] = {0};
        DWORD len = GetEnvironmentVariableA("crossforge_PROCESS_TIMEOUT_MS", envBuf, static_cast<DWORD>(sizeof(envBuf)));
        if (len == 0 || len >= sizeof(envBuf)) {
            return defaultMs;
        }

        try {
            unsigned long parsed = std::stoul(std::string(envBuf, len));
            return static_cast<DWORD>((std::max)(static_cast<unsigned long>(minMs), (std::min)(parsed, static_cast<unsigned long>(maxMs))));
        } catch (const std::exception&) {
            return defaultMs;
        } catch (...) {
            return defaultMs;
        }
    }

    static DWORD GetActiveProcessLimit() {
        const DWORD defaultLimit = 64;
        const DWORD minLimit = 1;
        const DWORD maxLimit = 512;

        char envBuf[64] = {0};
        DWORD len = GetEnvironmentVariableA("crossforge_PROCESS_MAX_CHILDREN", envBuf, static_cast<DWORD>(sizeof(envBuf)));
        if (len == 0 || len >= sizeof(envBuf)) {
            return defaultLimit;
        }

        try {
            unsigned long parsed = std::stoul(std::string(envBuf, len));
            return static_cast<DWORD>((std::max)(static_cast<unsigned long>(minLimit), (std::min)(parsed, static_cast<unsigned long>(maxLimit))));
        } catch (const std::exception&) {
            return defaultLimit;
        } catch (...) {
            return defaultLimit;
        }
    }

    static std::string ResolveShellPath() {
        char sysDir[MAX_PATH] = {0};
        UINT sysLen = GetSystemDirectoryA(sysDir, MAX_PATH);
        if (sysLen > 0 && sysLen < MAX_PATH) {
            return std::string(sysDir, sysLen) + "\\cmd.exe";
        }
        return "C:\\Windows\\System32\\cmd.exe";
    }

    static std::string StripCmdPrefix(const std::string& command) {
        auto trim = [](std::string s) {
            size_t start = 0;
            while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
            size_t end = s.size();
            while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
            return s.substr(start, end - start);
        };

        auto toLower = [](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        };

        std::string trimmed = trim(command);
        if (trimmed.empty()) {
            return trimmed;
        }

        size_t tokenEnd = 0;
        if (!trimmed.empty() && trimmed[0] == '"') {
            size_t quoteEnd = trimmed.find('"', 1);
            tokenEnd = (quoteEnd == std::string::npos) ? trimmed.size() : quoteEnd + 1;
        } else {
            while (tokenEnd < trimmed.size() && !std::isspace(static_cast<unsigned char>(trimmed[tokenEnd]))) {
                ++tokenEnd;
            }
        }

        std::string token = trimmed.substr(0, tokenEnd);
        std::string tokenLower = toLower(token);

        bool isCmd = (tokenLower == "cmd" || tokenLower == "cmd.exe" ||
                      tokenLower.find("\\cmd.exe") != std::string::npos ||
                      tokenLower.find("/cmd.exe") != std::string::npos);
        if (!isCmd) {
            return trimmed;
        }

        std::string rest = trim(trimmed.substr(tokenEnd));
        // Strip cmd switches that precede the command string: /c, and any /v:on
        // (delayed-expansion) flag that may appear before /c.
        while (rest.size() >= 2 && rest[0] == '/') {
            char c1 = static_cast<char>(std::tolower(static_cast<unsigned char>(rest[1])));
            if (c1 == 'c') { rest = trim(rest.substr(2)); break; }
            // Skip a non-/c switch token (e.g. /v:on, /d) and keep scanning.
            size_t sp = 1;
            while (sp < rest.size() && !std::isspace(static_cast<unsigned char>(rest[sp]))) ++sp;
            rest = trim(rest.substr(sp));
        }
        return rest;
    }

    // Non-destructively check whether the user pressed ESC on stdin, and consume
    // it if so. Handles both real console input handles (conhost/Windows
    // Terminal) and pipe/PTY stdin.
    static bool CheckEscapeCancel(HANDLE inputHandle) {
        if (inputHandle == INVALID_HANDLE_VALUE || inputHandle == nullptr) return false;

        if (GetFileType(inputHandle) == FILE_TYPE_CHAR) {
            // Console input buffer: PeekNamedPipe fails here, so use console APIs.
            DWORD numEvents = 0;
            if (!GetNumberOfConsoleInputEvents(inputHandle, &numEvents) || numEvents == 0) {
                return false;
            }
            std::vector<INPUT_RECORD> buf(numEvents);
            DWORD peeked = 0;
            if (!PeekConsoleInputW(inputHandle, buf.data(), numEvents, &peeked)) return false;
            for (DWORD i = 0; i < peeked; ++i) {
                const INPUT_RECORD& rec = buf[i];
                if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown &&
                    rec.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE) {
                    // Consume events up to and including the ESC so it doesn't leak
                    // into the editor's input queue after the process exits.
                    std::vector<INPUT_RECORD> discard(i + 1);
                    DWORD consumed = 0;
                    ReadConsoleInputW(inputHandle, discard.data(), i + 1, &consumed);
                    return true;
                }
            }
            return false;
        }

        // Pipe / PTY stdin: ESC arrives as a raw 0x1B byte.
        DWORD avail = 0;
        if (!PeekNamedPipe(inputHandle, nullptr, 0, nullptr, &avail, nullptr) || avail == 0) {
            return false;
        }
        unsigned char firstByte = 0;
        DWORD peekRead = 0;
        if (PeekNamedPipe(inputHandle, &firstByte, 1, &peekRead, nullptr, nullptr) &&
            peekRead > 0 && firstByte == 0x1B) {
            char discard;
            DWORD consumed = 0;
            ReadFile(inputHandle, &discard, 1, &consumed, nullptr);
            return true;
        }
        return false;
    }

    static bool RunProcess(const std::string& command, std::string& outputText) {
        // Create a restricted Windows Job Object for Sandboxing child processes
        HANDLE hJob = CreateJobObjectW(NULL, NULL);
        if (hJob) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli = { 0 };
            jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            jeli.BasicLimitInformation.ActiveProcessLimit = GetActiveProcessLimit();
            if (jeli.BasicLimitInformation.ActiveProcessLimit > 0) {
                jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
            }
            SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        }

        SECURITY_ATTRIBUTES saAttr{};
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        HANDLE hReadPipe, hWritePipe;
        if (!CreatePipe(&hReadPipe, &hWritePipe, &saAttr, 0)) {
            if (hJob) CloseHandle(hJob);
            return false;
        }
        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOW si{};
        si.cb = sizeof(STARTUPINFOW);
        si.hStdOutput = hWritePipe;
        si.hStdError  = hWritePipe;
        si.dwFlags |= STARTF_USESTDHANDLES;

        PROCESS_INFORMATION pi{};
        const std::string shellPath = ResolveShellPath();
        const std::string shellCommand = "\"" + shellPath + "\" /d /c " + StripCmdPrefix(command);
        std::wstring shellPathW = Utf8ToWide(shellPath);
        std::wstring shellCommandW = Utf8ToWide(shellCommand);
        // CreateProcessW requires a writable command-line buffer.
        std::vector<wchar_t> cmdBuf(shellCommandW.begin(), shellCommandW.end());
        cmdBuf.push_back(L'\0');

        BOOL success = CreateProcessW(
            shellPathW.empty() ? nullptr : shellPathW.c_str(), cmdBuf.data(), NULL, NULL, TRUE,
            CREATE_SUSPENDED | CREATE_NO_WINDOW, NULL, NULL, &si, &pi
        );

        if (!success) {
            outputText += "\n[CrossForge] Failed to launch process (CreateProcessW error " + std::to_string(GetLastError()) + ").";
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            if (hJob) CloseHandle(hJob);
            return false;
        }

        if (hJob && !AssignProcessToJobObject(hJob, pi.hProcess)) {
            outputText += "\n[CrossForge] Failed to assign process to sandbox job object.";
            TerminateProcess(pi.hProcess, 125);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            CloseHandle(hJob);
            return false;
        }

        if (ResumeThread(pi.hThread) == static_cast<DWORD>(-1)) {
            outputText += "\n[CrossForge] Failed to start process thread.";
            TerminateProcess(pi.hProcess, 126);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            CloseHandle(hReadPipe);
            CloseHandle(hWritePipe);
            if (hJob) CloseHandle(hJob);
            return false;
        }

        CloseHandle(hWritePipe);

        const DWORD timeoutMs = GetProcessTimeoutMs();
        ULONGLONG deadline = GetTickCount64() + timeoutMs;
        bool timedOut = false;
        bool cancelled = false;
        bool readError = false;

        // Event-driven wait: a reader thread performs blocking ReadFile on the
        // child's pipe and signals hPipeDataEvent whenever output arrives (or the
        // pipe closes). The main thread waits on { process exit, pipe data } with
        // a bounded slice so ESC-cancel and the deadline stay responsive without
        // a Sleep() spin loop.
        HANDLE hPipeDataEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr); // auto-reset
        struct PipeReaderState {
            HANDLE pipe = nullptr;
            HANDLE dataEvent = nullptr;
            std::string* output = nullptr;
            std::atomic<bool> stop{ false };
            std::atomic<bool> readError{ false };
            std::mutex mtx;
        };
        PipeReaderState readerState{ hReadPipe, hPipeDataEvent, &outputText, {}, {}, {} };

        std::thread pipeReader([&readerState]() {
            char buffer[1024];
            while (!readerState.stop.load()) {
                DWORD bytesRead = 0;
                BOOL okRead = ReadFile(readerState.pipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr);
                if (!okRead || bytesRead == 0) {
                    // Pipe closed (child exited / error). Signal so the waiter wakes.
                    if (readerState.dataEvent) SetEvent(readerState.dataEvent);
                    if (!okRead) readerState.readError.store(true);
                    break;
                }
                buffer[bytesRead] = '\0';
                {
                    std::lock_guard<std::mutex> lock(readerState.mtx);
                    readerState.output->append(buffer, static_cast<size_t>(bytesRead));
                }
                if (readerState.dataEvent) SetEvent(readerState.dataEvent);
            }
        });

        auto flushPipeOutput = [&]() {
            std::lock_guard<std::mutex> lock(readerState.mtx);
            // Output already appended by the reader thread; nothing to do here.
            // (kept for symmetry / future per-chunk processing)
        };

        HANDLE waitHandles[2] = { pi.hProcess, hPipeDataEvent };
        while (true) {
            // Compute remaining time so we wake promptly at the deadline even if
            // no events fire; 50 ms slices keep ESC-cancel responsive.
            ULONGLONG now = GetTickCount64();
            DWORD slice = 50;
            if (now < deadline) {
                ULONGLONG remaining = deadline - now;
                if (remaining < slice) slice = static_cast<DWORD>(remaining);
            } else {
                slice = 0;
            }

            DWORD waitResult = WaitForMultipleObjects(2, waitHandles, FALSE, slice);
            flushPipeOutput();

            if (waitResult == WAIT_OBJECT_0) {
                // Process exited. Let the reader finish draining, then stop.
                break;
            }
            if (waitResult == WAIT_FAILED) {
                outputText += "\n[CrossForge] Failed waiting on process handle.";
                readError = true;
                break;
            }
            // WAIT_OBJECT_0+1 (pipe data) or WAIT_TIMEOUT: check cancel/deadline.

            // ESC-cancel. stdin can be a real console input handle (conhost /
            // Windows Terminal, FILE_TYPE_CHAR) or a pipe/PTY. PeekNamedPipe only
            // works on pipes, so pick the right peek per handle type.
            HANDLE inputHandle = GetStdHandle(STD_INPUT_HANDLE);
            if (inputHandle != INVALID_HANDLE_VALUE && CheckEscapeCancel(inputHandle)) {
                cancelled = true;
                outputText += "\n[CrossForge] Process cancelled by user.";
                TerminateProcess(pi.hProcess, 123);
                WaitForSingleObject(pi.hProcess, 2000);
                break;
            }

            if (GetTickCount64() >= deadline) {
                timedOut = true;
                outputText += "\n[CrossForge] Process timed out after " + std::to_string(timeoutMs) + " ms.";
                TerminateProcess(pi.hProcess, 124);
                WaitForSingleObject(pi.hProcess, 2000);
                break;
            }
        }

        // Stop and join the reader thread, then close the event.
        readerState.stop.store(true);
        // Nudge the reader out of a blocking ReadFile if the child is gone but
        // the pipe is somehow still open: cancel pending I/O on this pipe.
        CancelIoEx(hReadPipe, nullptr);
        if (pipeReader.joinable()) pipeReader.join();
        if (readerState.readError.load()) readError = true;
        if (hPipeDataEvent) CloseHandle(hPipeDataEvent);

        bool ok = false;
        if (!timedOut && !cancelled && !readError) {
            DWORD exitCode = 1;
            if (GetExitCodeProcess(pi.hProcess, &exitCode)) {
                ok = (exitCode == 0);
                if (!ok) {
                    outputText += "\n[CrossForge] Process exited with code " + std::to_string(exitCode) + ".";
                }
            } else {
                outputText += "\n[CrossForge] Failed to query process exit code.";
            }
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);
        if (hJob) CloseHandle(hJob);

        return ok;
    }
};

// ============================================================================
// DOCUMENT & EDITOR SYSTEM
// ============================================================================

// Cross-line lexical state for the syntax highlighter. Carried from line to
// line so block comments and multi-line strings stay highlighted.
struct HighlightState {
    bool inBlockComment = false; // inside /* ... */
    char stringQuote = '\0';     // inside a string: '"' or '\'' ('\0' = none)
    bool operator==(const HighlightState& o) const {
        return inBlockComment == o.inBlockComment && stringQuote == o.stringQuote;
    }
    bool operator!=(const HighlightState& o) const { return !(*this == o); }
};

// Advance `state` by scanning a whole line without rendering. Handles // and #
// line comments, /* */ block comments, and strings (with backslash escapes, so
// an escaped quote doesn't end a string). A string open at EOL remains open.
inline void ScanHighlightLineState(const std::string& line, HighlightState& state) {
    size_t idx = 0;
    const size_t n = line.length();
    while (idx < n) {
        if (state.inBlockComment) {
            size_t close = line.find("*/", idx);
            if (close == std::string::npos) { return; } // still in comment at EOL
            state.inBlockComment = false;
            idx = close + 2;
            continue;
        }
        if (state.stringQuote != '\0') {
            char q = state.stringQuote;
            char c = line[idx];
            if (c == '\\' && idx + 1 < n) { idx += 2; continue; } // skip escape
            if (c == q) { state.stringQuote = '\0'; }
            ++idx;
            continue;
        }
        // Not in a continuing construct.
        if (line[idx] == '/' && idx + 1 < n && line[idx + 1] == '/') {
            return; // line comment: rest of line, state unchanged
        }
        if (line[idx] == '#') {
            return;
        }
        if (line[idx] == '/' && idx + 1 < n && line[idx + 1] == '*') {
            state.inBlockComment = true;
            idx += 2;
            continue;
        }
        if (line[idx] == '"' || line[idx] == '\'') {
            state.stringQuote = line[idx];
            ++idx;
            continue;
        }
        ++idx;
    }
}

// Persistent line buffer: each line is an immutable shared_ptr<const string>.
// Snapshots (undo/redo) copy only the pointer vector, so unchanged lines are
// shared across history entries instead of being deep-copied on every keystroke.
// Edits always replace a line slot with a fresh string (copy-on-write), which
// keeps history snapshots valid.
class Document {
public:
    using LinePtr = std::shared_ptr<const std::string>;

    // Line storage wrapper that mimics the subset of std::vector<std::string>
    // used by the editor while storing lines as immutable shared pointers.
    class LineBuffer {
    public:
        size_t size() const { return m_lines.size(); }
        bool empty() const { return m_lines.empty(); }

        // Reading a line yields a const std::string& (zero-copy). Returns a static
        // empty string for an out-of-range index instead of UB.
        const std::string& operator[](size_t i) const {
            static const std::string kEmpty;
            return (i < m_lines.size() && m_lines[i]) ? *m_lines[i] : kEmpty;
        }

        // Assigning replaces the slot with a fresh immutable line.
        void set(size_t i, std::string s) { m_lines[i] = std::make_shared<const std::string>(std::move(s)); }

        void push_back(const std::string& s) { m_lines.push_back(std::make_shared<const std::string>(s)); }

        void insert(size_t index, const std::string& s) {
            if (index > m_lines.size()) index = m_lines.size();
            m_lines.insert(m_lines.begin() + index, std::make_shared<const std::string>(s));
        }

        void erase(size_t index) {
            if (index < m_lines.size()) m_lines.erase(m_lines.begin() + index);
        }

        void clear() { m_lines.clear(); }

        void assignSingle(const std::string& s) {
            m_lines.clear();
            push_back(s);
        }

        void resetSingle(const std::string& s) { assignSingle(s); }

        // Replace every line with a fresh string (used by replace-all).
        void replaceEach(const std::function<std::string(const std::string&)>& fn) {
            for (size_t i = 0; i < m_lines.size(); ++i) {
                set(i, fn(*m_lines[i]));
            }
        }

        // Approximate live byte size of all line content.
        size_t byteSize() const {
            size_t total = 0;
            for (const auto& p : m_lines) total += p ? p->size() : 0;
            return total;
        }

    private:
        std::vector<LinePtr> m_lines;
    };

    struct Snapshot {
        LineBuffer Lines;
        int CursorX = 0;
        int CursorY = 0;
        int ScrollY = 0;
        int ScrollX = 0;
        bool IsModified = false;
    };

    std::string FilePath;
    std::string Title;
    LineBuffer Lines;
    int CursorX = 0;
    int CursorY = 0;
    int ScrollY = 0;
    int ScrollX = 0;
    bool IsModified = false;
    SupportedLanguage Lang = SupportedLanguage::Text;
    int WindowId = 1;
    std::deque<Snapshot> UndoStack;
    std::deque<Snapshot> RedoStack;

    // Incremental syntax-highlight line-state cache. m_lineStates[i] is the
    // HighlightState at the START of line i. m_stateDirtyFrom is the first line
    // whose entry state may have changed; lines before it are known-good, so a
    // query for the state at line L rescans only from m_stateDirtyFrom, not from 0.
    mutable std::vector<HighlightState> m_lineStates;
    mutable size_t m_stateDirtyFrom = 0;

    // Mark that lines from `index` onward may have changed, invalidating their
    // cached entry states (cheap: just lowers the dirty watermark).
    void InvalidateLineStatesFrom(size_t index) const {
        if (index < m_stateDirtyFrom) m_stateDirtyFrom = index;
    }

    // Return the HighlightState at the start of line `line`, rescanning only from
    // the first dirty line. Amortized O(1) per frame when edits are localized.
    HighlightState GetLineState(size_t line) const {
        size_t count = Lines.size();
        if (m_lineStates.size() < count + 1) m_lineStates.resize(count + 1);
        size_t from = (std::min)(m_stateDirtyFrom, line);
        HighlightState s = m_lineStates[from];
        for (size_t i = from; i < line; ++i) {
            ScanHighlightLineState(Lines[i], s);
            m_lineStates[i + 1] = s;
        }
        // Everything up to `line` is now known-good.
        if (m_stateDirtyFrom < line) m_stateDirtyFrom = line;
        return s;
    }

    Document(int id, const std::string& path = "") : WindowId(id), FilePath(path) {
        if (path.empty()) {
            Title = "NO NAME " + std::to_string(id) + ".TXT";
            Lines.push_back("");
        } else {
            Title = fs::path(path).filename().string();
            LoadFile(path);
        }
    }

    void LoadFile(const std::string& path) {
        FilePath = path;
        Lines.clear();
        std::ifstream file(path, std::ios::binary);
        if (file.is_open()) {
            std::string line;
            bool firstLine = true;
            while (std::getline(file, line)) {
                // Strip a UTF-8 BOM from the first line so it isn't saved back as content.
                if (firstLine && line.size() >= 3 &&
                    static_cast<unsigned char>(line[0]) == 0xEF &&
                    static_cast<unsigned char>(line[1]) == 0xBB &&
                    static_cast<unsigned char>(line[2]) == 0xBF) {
                    line = line.substr(3);
                }
                // Drop a trailing CR so CRLF files don't carry \r into the buffer.
                if (!line.empty() && line.back() == '\r') line.pop_back();
                Lines.push_back(line);
                firstLine = false;
            }
            file.close();
        }
        if (Lines.empty()) Lines.push_back("");
        // Fresh content: drop any cached highlight line-states.
        m_lineStates.clear();
        m_stateDirtyFrom = 0;
    }

    // Save the buffer. Returns true only if the file was fully written; leaves
    // IsModified set on failure so the user isn't told a bad save succeeded.
    bool SaveFile() {
        if (FilePath.empty()) return false;
        std::ofstream file(FilePath, std::ios::binary | std::ios::trunc);
        if (!file.is_open()) return false;
        for (size_t i = 0; i < Lines.size(); ++i) {
            file << Lines[i];
            if (i + 1 < Lines.size()) file << "\n";
        }
        file.flush();
        if (file.fail()) {
            file.close();
            return false; // keep IsModified set; caller can warn the user
        }
        file.close();
        IsModified = false;
        return true;
    }

    void RecordEdit() {
        // Snapshot copies only shared line pointers (O(line count), not O(bytes)).
        UndoStack.push_back(Snapshot{Lines, CursorX, CursorY, ScrollY, ScrollX, IsModified});
        if (UndoStack.size() > 200) {
            UndoStack.pop_front();
        }
        RedoStack.clear();
    }

    bool Undo() {
        if (UndoStack.empty()) return false;
        RedoStack.push_back(Snapshot{Lines, CursorX, CursorY, ScrollY, ScrollX, IsModified});
        Snapshot state = std::move(UndoStack.back());
        UndoStack.pop_back();
        Lines = std::move(state.Lines);
        CursorX = state.CursorX;
        CursorY = state.CursorY;
        ScrollY = state.ScrollY;
        ScrollX = state.ScrollX;
        IsModified = state.IsModified; // restore, don't force-modified
        InvalidateLineStatesFrom(0);
        return true;
    }

    bool Redo() {
        if (RedoStack.empty()) return false;
        UndoStack.push_back(Snapshot{Lines, CursorX, CursorY, ScrollY, ScrollX, IsModified});
        Snapshot state = std::move(RedoStack.back());
        RedoStack.pop_back();
        Lines = std::move(state.Lines);
        CursorX = state.CursorX;
        CursorY = state.CursorY;
        ScrollY = state.ScrollY;
        ScrollX = state.ScrollX;
        IsModified = state.IsModified; // restore, don't force-modified
        InvalidateLineStatesFrom(0);
        return true;
    }
};

// ============================================================================
// TUI WINDOW & RENDERING ENGINE
// ============================================================================

class CrossForgeEngine {
private:
    struct MenuEntry {
        std::string Label;
        int CommandId;
    };

    enum class MenuCommand {
        None = 0,
        FileNew,
        FileOpen,
        FileSave,
        FileSaveAs,
        FileClose,
        FileExit,
        FileRecent,
        EditInsertLine,
        EditDeleteLine,
        EditDuplicateLine,
        EditUndo,
        EditRedo,
        EditCut,
        EditCopy,
        EditPaste,
        EditSelectAll,
        SearchFind,
        SearchFindNext,
        SearchFindPrevious,
        SearchReplace,
        RunTerminal,
        RunWslTerminal,
        RunSshTerminal,
        ViewCompilerOutput,
        ViewNextDiagnostic,
        CompileBuild,
        CompileRun,
        CompileTest,
        BuildDetect,
        GitStatus,
        GitAddAll,
        GitCommit,
        GitPull,
        GitPush,
        GitLog,
        GitDiff,
        DebugRunActive,
        DebugViewOutput,
        DebugDetect,
        OptionsNextTheme,
        OptionsAddCompilerPath,
        OptionsRunLinker,
        WindowNext,
        WindowNew,
        WindowLayoutToggle,
        HelpTopics,
        HelpDiagnostics,
        HelpAbout
    };

    HANDLE m_hStdOut;
    HANDLE m_hStdIn;
    DWORD m_defaultInputMode = 0;
    DWORD m_defaultOutputMode = 0;
    bool m_useClassicConsoleInput = false;
    CONSOLE_SCREEN_BUFFER_INFO m_csbi;
    int m_width;
    int m_height;

    std::vector<CHAR_INFO> m_screenBuffer;
    std::string m_inBuf;      // VT input accumulator (raw bytes from stdin)
    size_t m_inPos = 0;       // parse cursor into m_inBuf
    bool m_cursorVisible = true; // tracked for VT cursor show/hide
    std::vector<std::shared_ptr<Document>> m_documents;
    int m_activeDocIndex = 0;

    enum class TopMenuAction {
        None,
        File,
        Edit,
        Search,
        Run,
        Compile,
        Git,
        Debug,
        Options,
        Window,
        Help
    };

    ThemeScheme m_currentTheme = Theme_BorlandBlue;
    ThemePalette m_palette;
    LanguageRegistry m_langRegistry;
    TopMenuAction m_pressedTopMenuAction = TopMenuAction::None;
    std::string m_statusMessage;
    std::string m_clipboardText;
    std::string m_lastToolTitle;
    std::string m_lastToolCommand;
    std::string m_lastToolOutput;
    bool m_hasLastToolOutput = false;
    std::string m_lastDebugTitle;
    std::string m_lastDebugCommand;
    std::string m_lastDebugOutput;
    bool m_hasLastDebugOutput = false;
    bool m_selectAllActive = false;
    bool m_needsRender = true;
    std::string m_lastSearchTerm;
    std::vector<std::string> m_recentFiles;
    size_t m_recentIndex = 0;
    struct Diagnostic {
        std::string FilePath;
        int Line = 0;
        std::string Message;
    };
    std::vector<Diagnostic> m_diagnostics;
    size_t m_diagnosticIndex = 0;

    // Window layout mode: Split tiles all open documents evenly, Maximized gives
    // the active document the full editor area, and Tabs shows a tab bar across
    // the top with the active document filling the rest.
    enum class WindowLayout { Split, Maximized, Tabs };
    WindowLayout m_windowLayout = WindowLayout::Split;

    bool m_running = true;
    bool m_terminalMode = false;

public:
    CrossForgeEngine() {
        m_hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
        m_hStdIn  = GetStdHandle(STD_INPUT_HANDLE);

        // Prevent raw Ctrl+C from terminating the IDE process.
        SetConsoleCtrlHandler(&CrossForgeEngine::ConsoleCtrlHandler, TRUE);

        // UTF-8 Console Code Page Setup
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);

        // Setup and remember console modes so we can restore cleanly after terminal sessions.
        GetConsoleMode(m_hStdIn, &m_defaultInputMode);
        GetConsoleMode(m_hStdOut, &m_defaultOutputMode);
        m_useClassicConsoleInput = GetFileType(m_hStdIn) == FILE_TYPE_CHAR;
        ApplyEditorConsoleModes();
        EnsureVisualStudioBuildToolsInPath();

        m_palette = ThemeManager::GetTheme(m_currentTheme);
        UpdateBufferSize();

        // Start with a single clean buffer.
        auto doc = std::make_shared<Document>(1, "");
        doc->Lines.resetSingle("");
        doc->Lang = SupportedLanguage::Text;
        doc->FilePath = "";
        doc->Title = "NO NAME";
        m_documents.push_back(doc);
        LoadUserProject();
    }

    ~CrossForgeEngine() {
        SaveUserProject();
        SetConsoleCtrlHandler(&CrossForgeEngine::ConsoleCtrlHandler, FALSE);
        CleanupConsoleOnExit();
    }

    void Run() {
        while (m_running) {
            bool sizeChanged = UpdateBufferSize();
            if (sizeChanged || m_needsRender) {
                RenderScreen();
                m_needsRender = false;
            }
            ProcessInput();
        }
    }

private:
    static fs::path GetUserProjectPath() {
        return GetUserHomePath() / ".crossforge";
    }

    static fs::path GetLegacyUserProjectPath() {
        return GetUserHomePath() / ".codeforge";
    }

    static fs::path GetUserHomePath() {
        std::string home = ReadEnvironmentVariableString("USERPROFILE");
        if (home.empty()) {
            home = ReadEnvironmentVariableString("HOMEDRIVE") + ReadEnvironmentVariableString("HOMEPATH");
        }
        if (home.empty()) {
            home = fs::current_path().string();
        }
        return fs::path(home);
    }

    static std::string GetLocalTimestamp() {
        std::time_t now = std::time(nullptr);
        std::tm localTime{};
        localtime_s(&localTime, &now);

        char timestamp[32]{};
        std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &localTime);
        return timestamp;
    }

    static void LogCompilerOutput(const std::string& title, const std::string& command,
                                  const std::string& output, bool ok) {
        std::ofstream log(GetUserHomePath() / ".crossforge_compiler_output", std::ios::app);
        if (!log.is_open()) return;

        log << "===== " << GetLocalTimestamp() << " | " << title
            << " | " << (ok ? "OK" : "FAILED") << " =====\n";
        log << "Command: " << command << "\n";
        log << output;
        if (output.empty() || output.back() != '\n') log << '\n';
        log << "\n";
    }

    void RememberRecentFile(const std::string& path) {
        if (path.empty()) return;
        m_recentFiles.erase(std::remove(m_recentFiles.begin(), m_recentFiles.end(), path), m_recentFiles.end());
        m_recentFiles.insert(m_recentFiles.begin(), path);
        m_recentIndex = 0;
        if (m_recentFiles.size() > 10) m_recentFiles.resize(10);
    }

    void LoadUserProject() {
        std::ifstream file(GetUserProjectPath());
        if (!file.is_open()) {
            file.open(GetLegacyUserProjectPath());
        }
        if (!file.is_open()) return;

        std::vector<std::string> paths;
        std::string line;
        while (std::getline(file, line)) {
            if (line.rfind("THEME ", 0) == 0) {
                try {
                    int theme = std::stoi(line.substr(6));
                    if (theme >= static_cast<int>(Theme_BorlandBlue) && theme <= static_cast<int>(Theme_RetroAmber)) {
                        m_currentTheme = static_cast<ThemeScheme>(theme);
                        m_palette = ThemeManager::GetTheme(m_currentTheme);
                    }
                } catch (...) {
                }
            } else if (line.rfind("ACTIVE ", 0) == 0) {
                try { m_activeDocIndex = std::stoi(line.substr(7)); } catch (...) { }
            } else if (line.rfind("FILE ", 0) == 0) {
                paths.push_back(line.substr(5));
            } else if (line.rfind("RECENT ", 0) == 0) {
                RememberRecentFile(line.substr(7));
            }
        }

        std::vector<std::shared_ptr<Document>> loaded;
        for (const auto& path : paths) {
            if (!path.empty() && fs::exists(path) && fs::is_regular_file(path)) {
                auto doc = std::make_shared<Document>(static_cast<int>(loaded.size()) + 1, path);
                doc->Lang = m_langRegistry.DetectLanguage(path);
                loaded.push_back(doc);
            }
        }

        if (!loaded.empty()) {
            m_documents = std::move(loaded);
            m_activeDocIndex = (std::max)(0, (std::min)(m_activeDocIndex, static_cast<int>(m_documents.size()) - 1));
        }
    }

    void SaveUserProject() const {
        std::ofstream file(GetUserProjectPath(), std::ios::trunc);
        if (!file.is_open()) return;

        file << "CROSSFORGE_PROJECT 1\n";
        file << "THEME " << static_cast<int>(m_currentTheme) << "\n";
        file << "ACTIVE " << m_activeDocIndex << "\n";
        for (const auto& doc : m_documents) {
            if (doc && !doc->FilePath.empty()) file << "FILE " << doc->FilePath << "\n";
        }
        for (const auto& path : m_recentFiles) {
            file << "RECENT " << path << "\n";
        }
    }

    static std::string Utf8FromWideChar(wchar_t wch) {
        if (wch == 0 || wch < 32) {
            return "";
        }

        wchar_t wbuf[2] = {wch, 0};
        int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wbuf, 1, NULL, 0, NULL, NULL);
        if (sizeNeeded <= 0) {
            return "";
        }

        std::string out(static_cast<size_t>(sizeNeeded), '\0');
        if (WideCharToMultiByte(CP_UTF8, 0, wbuf, 1, &out[0], sizeNeeded, NULL, NULL) <= 0) {
            return "";
        }
        return out;
    }

    static size_t PrevUtf8CharStart(const std::string& s, size_t pos) {
        // Return the byte index where the codepoint ending at/just before pos begins.
        if (pos == 0) return 0;
        size_t idx = pos - 1;
        while (idx > 0 && (static_cast<unsigned char>(s[idx]) & 0xC0) == 0x80) {
            --idx;
        }
        return idx;
    }

    static size_t NextUtf8CharEnd(const std::string& s, size_t pos) {
        // Return the byte index just past the codepoint starting at pos.
        if (pos >= s.size()) return s.size();
        size_t idx = pos + 1;
        while (idx < s.size() && (static_cast<unsigned char>(s[idx]) & 0xC0) == 0x80) {
            ++idx;
        }
        return idx;
    }

    static void PopLastUtf8Codepoint(std::string& s) {
        if (s.empty()) {
            return;
        }
        size_t idx = s.size() - 1;
        while (idx > 0 && (static_cast<unsigned char>(s[idx]) & 0xC0) == 0x80) {
            --idx;
        }
        s.erase(idx);
    }

    // Decode the codepoint starting at byte offset `pos`; returns its byte length
    // via `len`. Returns U+FFFD (len 1) on malformed input.
    static uint32_t DecodeUtf8Codepoint(const std::string& s, size_t pos, int& len) {
        if (pos >= s.size()) { len = 0; return 0xFFFD; } // entry bounds guard
        unsigned char b = static_cast<unsigned char>(s[pos]);
        if (b < 0x80) { len = 1; return b; }
        int extra = (b >= 0xF0) ? 3 : (b >= 0xE0) ? 2 : (b >= 0xC0) ? 1 : 0;
        // Need bytes pos..pos+extra inclusive; ensure the last index is in range.
        if (extra == 0 || pos + extra >= s.size()) { len = 1; return 0xFFFD; }
        uint32_t cp = b & ((1 << (6 - extra)) - 1);
        for (int k = 1; k <= extra; ++k) {
            unsigned char cb = static_cast<unsigned char>(s[pos + k]);
            if ((cb & 0xC0) != 0x80) { len = 1; return 0xFFFD; }
            cp = (cp << 6) | (cb & 0x3F);
        }
        len = extra + 1;
        return cp;
    }

    // wcwidth-style display width (in terminal cells) for a codepoint.
    static int CodepointDisplayWidth(uint32_t cp) {
        if (cp == 0) return 0;
        if (cp < 32 || (cp >= 0x7F && cp < 0xA0)) return 0;        // control chars
        if (cp < 0x1100) return 1;                                  // ASCII + Latin/Greek/etc.
        // Wide ranges (CJK, Hangul, full-width forms, most emoji).
        if ((cp >= 0x1100 && cp <= 0x115F) ||                       // Hangul Jamo
            (cp >= 0x2E80 && cp <= 0xA4CF) ||                       // CJK radicals .. Yi
            (cp >= 0xAC00 && cp <= 0xD7A3) ||                       // Hangul syllables
            (cp >= 0xF900 && cp <= 0xFAFF) ||                       // CJK compat ideographs
            (cp >= 0xFE30 && cp <= 0xFE4F) ||                       // CJK compat forms
            (cp >= 0xFF00 && cp <= 0xFF60) ||                       // Fullwidth forms
            (cp >= 0xFFE0 && cp <= 0xFFE6) ||                       // Fullwidth signs
            (cp >= 0x1F300 && cp <= 0x1FAFF) ||                     // Emoji & symbols
            (cp >= 0x20000 && cp <= 0x3FFFD)) {                     // CJK ext B+
            return 2;
        }
        return 1;
    }

    // Display width (cells) of a UTF-8 string. A tab advances to the next multiple
    // of the tab width (positional), matching how terminals render it.
    static constexpr int kTabWidth = 4;
    static int Utf8DisplayWidth(const std::string& s) {
        int width = 0;
        size_t i = 0;
        while (i < s.size()) {
            int len = 1;
            uint32_t cp = DecodeUtf8Codepoint(s, i, len);
            width += (cp == '\t') ? (kTabWidth - (width % kTabWidth)) : CodepointDisplayWidth(cp);
            i += static_cast<size_t>(len);
        }
        return width;
    }

    // Display width (cells) of the first `byteCount` bytes of a UTF-8 string.
    static int Utf8DisplayWidth(const std::string& s, size_t byteCount) {
        int width = 0;
        size_t i = 0;
        size_t limit = (std::min)(byteCount, s.size());
        while (i < limit) {
            int len = 1;
            uint32_t cp = DecodeUtf8Codepoint(s, i, len);
            if (i + static_cast<size_t>(len) > limit) break; // don't count partial cp
            width += (cp == '\t') ? (kTabWidth - (width % kTabWidth)) : CodepointDisplayWidth(cp);
            i += static_cast<size_t>(len);
        }
        return width;
    }

    // Convert a byte offset in `s` to the display column of its containing codepoint.
    static int ByteOffsetToDisplayCol(const std::string& s, int byteOffset) {
        int clamped = (std::max)(0, (std::min)(byteOffset, static_cast<int>(s.size())));
        // Snap back to a codepoint boundary first.
        while (clamped > 0 && clamped < static_cast<int>(s.size()) &&
               (static_cast<unsigned char>(s[clamped]) & 0xC0) == 0x80) {
            clamped--;
        }
        return Utf8DisplayWidth(s, static_cast<size_t>(clamped));
    }

    // Convert a display column to the byte offset of the codepoint at/after it.
    static int DisplayColToByteOffset(const std::string& s, int col) {
        if (col <= 0) return 0;
        int width = 0;
        size_t i = 0;
        while (i < s.size()) {
            int len = 1;
            uint32_t cp = DecodeUtf8Codepoint(s, i, len);
            int w = (cp == '\t') ? (kTabWidth - (width % kTabWidth)) : CodepointDisplayWidth(cp);
            if (width + w > col) break;         // target lands inside this codepoint/tab
            width += w;
            i += static_cast<size_t>(len);
            if (width == col) break;            // exactly at the next codepoint
        }
        return static_cast<int>(i);
    }

    static bool IsLikelyCompilerToolDirectory(const fs::path& dir) {
        static const char* kToolNames[] = {
            "cl.exe", "link.exe", "gcc.exe", "g++.exe", "clang.exe", "clang++.exe",
            "javac.exe", "csc.exe", "fpc.exe", "gfortran.exe", "cobc.exe", "swiftc.exe",
            "node.exe", "tsc.cmd"
        };

        for (const char* tool : kToolNames) {
            if (fs::exists(dir / tool)) {
                return true;
            }
        }
        return false;
    }

    static bool IsSafePathSegment(const std::string& value) {
        return !value.empty() && value.find(';') == std::string::npos && value.find('"') == std::string::npos;
    }

    void EnsureVisualStudioBuildToolsInPath() {
        std::string currentPath = ReadEnvironmentVariableString("PATH");

        auto normalize = [](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            while (!s.empty() && (s.back() == '\\' || s.back() == '/')) s.pop_back();
            return s;
        };

        std::vector<std::string> normalizedExisting;
        std::stringstream ss(currentPath);
        std::string segment;
        while (std::getline(ss, segment, ';')) {
            if (!segment.empty()) {
                normalizedExisting.push_back(normalize(segment));
            }
        }

        std::vector<std::string> additions;
        std::vector<std::string> vsDirs = CompilerDetector::GetVisualStudioBuildToolDirs();
        for (const auto& dir : vsDirs) {
            if (!IsSafePathSegment(dir)) {
                continue;
            }
            fs::path dirPath(dir);
            if (!fs::exists(dirPath) || !fs::is_directory(dirPath) || !IsLikelyCompilerToolDirectory(dirPath)) {
                continue;
            }

            std::string dirNorm = normalize(dir);
            if (std::find(normalizedExisting.begin(), normalizedExisting.end(), dirNorm) == normalizedExisting.end()) {
                normalizedExisting.push_back(dirNorm);
                additions.push_back(dir);
            }
        }

        if (additions.empty()) {
            return;
        }

        std::string updatedPath = currentPath;
        for (const auto& dir : additions) {
            if (!updatedPath.empty() && updatedPath.back() != ';') {
                updatedPath.push_back(';');
            }
            updatedPath += dir;
        }

        if (!SetEnvironmentVariableA("PATH", updatedPath.c_str())) {
            m_statusMessage = "Warning: failed to extend PATH with VS Build Tools.";
        }
    }

    static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
        if (ctrlType == CTRL_C_EVENT) {
            return TRUE;
        }
        return FALSE;
    }

    void CleanupConsoleOnExit() {
        if (m_hStdOut == INVALID_HANDLE_VALUE || m_hStdIn == INVALID_HANDLE_VALUE) {
            return;
        }

        SetConsoleMode(m_hStdIn, m_defaultInputMode);
        SetConsoleMode(m_hStdOut, m_defaultOutputMode);

        // VT-friendly teardown: reset attributes, show the cursor, clear the
        // screen, and move home. Works on a real console and over a PTY.
        static const char* kTeardown = "\x1b[0m\x1b[?25h\x1b[2J\x1b[H";
        DWORD written = 0;
        WriteFile(m_hStdOut, kTeardown, (DWORD)strlen(kTeardown), &written, nullptr);
    }

    void ApplyEditorConsoleModes() {
        // Native console handles produce INPUT_RECORDs; pipes/PTYs produce VT bytes.
        DWORD inputMode = (m_defaultInputMode | ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS) & ~ENABLE_QUICK_EDIT_MODE;
        if (!m_useClassicConsoleInput) {
            inputMode |= 0x0200 /*ENABLE_VIRTUAL_TERMINAL_INPUT*/;
        }
        inputMode &= ~ENABLE_ECHO_INPUT;
        inputMode &= ~ENABLE_LINE_INPUT;
        SetConsoleMode(m_hStdIn, inputMode);

        // Output: enable VT processing so the host interprets ANSI sequences.
        DWORD outputMode = m_defaultOutputMode;
        outputMode |= 0x0004 /*ENABLE_VIRTUAL_TERMINAL_PROCESSING*/;
        outputMode |= 0x0008 /*DISABLE_NEWLINE_AUTO_RETURN*/;
        SetConsoleMode(m_hStdOut, outputMode);
    }

    bool UpdateBufferSize() {
        int oldWidth = m_width;
        int oldHeight = m_height;
        if (GetConsoleScreenBufferInfo(m_hStdOut, &m_csbi)) {
            m_width  = m_csbi.srWindow.Right - m_csbi.srWindow.Left + 1;
            m_height = m_csbi.srWindow.Bottom - m_csbi.srWindow.Top + 1;
        }
        // Over a PTY the console API may fail; fall back to the last known or a
        // sensible default. (Full CSI 18t size reporting can be added later.)
        if (m_width <= 0)  m_width = 80;
        if (m_height <= 0) m_height = 24;
        m_screenBuffer.resize(m_width * m_height);
        return m_width != oldWidth || m_height != oldHeight;
    }

    void PutChar(int x, int y, wchar_t ch, WORD attr) {
        if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
            m_screenBuffer[y * m_width + x].Char.UnicodeChar = ch;
            m_screenBuffer[y * m_width + x].Attributes = attr;
        }
    }

    void DrawString(int x, int y, const std::string& str, WORD attr) {
        if (str.empty()) {
            return;
        }

        int wlen = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
        if (wlen <= 0) {
            return;
        }

        std::vector<wchar_t> wideText(static_cast<size_t>(wlen));
        if (MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, wideText.data(), wlen) <= 0) {
            return;
        }

        for (size_t i = 0; i + 1 < wideText.size(); ++i) {
            PutChar(x + static_cast<int>(i), y, wideText[i], attr);
        }
    }

    // VT cursor visibility: emit show/hide cursor escape. We track the last known
    // state ourselves since a PTY has no queryable console cursor info.
    bool SetCursorVisible(bool visible) {
        bool previous = m_cursorVisible;
        m_cursorVisible = visible;
        const char* seq = visible ? "\x1b[?25h" : "\x1b[?25l";
        DWORD written = 0;
        WriteFile(m_hStdOut, seq, (DWORD)strlen(seq), &written, nullptr);
        return previous;
    }

    // VT cursor position (1-based row/col in the escape sequence).
    void SetCursorPositionVT(int x, int y) {
        char esc[32];
        int n = snprintf(esc, sizeof(esc), "\x1b[%d;%dH", y + 1, x + 1);
        if (n > 0) {
            DWORD written = 0;
            WriteFile(m_hStdOut, esc, (DWORD)n, &written, nullptr);
        }
    }

    void ClearBuffer(WORD attr) {
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                PutChar(x, y, L' ', attr);
            }
        }
    }

    // ========================================================================
    // VT/ANSI TERMINAL INPUT BACKEND
    // Reads the stdin byte stream and decodes VT input sequences (arrows,
    // Home/End/PgUp/PgDn, F-keys, SGR mouse, and UTF-8 text) into the same
    // INPUT_RECORD structures the classic console path produced, so all of the
    // existing dispatch/dialog logic works unchanged over SSH and PTYs.
    // ========================================================================

    // Pull more bytes from stdin into the input accumulator. Returns false on EOF.
    bool FillInputBuffer() {
        if (m_inPos < m_inBuf.size()) return true;
        char buf[256];
        DWORD read = 0;
        if (!ReadFile(m_hStdIn, buf, sizeof(buf), &read, nullptr) || read == 0) return false;
        m_inBuf.assign(buf, buf + read);
        m_inPos = 0;
        return true;
    }

    // Read one raw byte; returns -1 when no byte is available.
    int ReadByte() {
        if (m_inPos >= m_inBuf.size() && !FillInputBuffer()) return -1;
        return static_cast<unsigned char>(m_inBuf[m_inPos++]);
    }

    int PeekByte() {
        if (m_inPos >= m_inBuf.size() && !FillInputBuffer()) return -1;
        return static_cast<unsigned char>(m_inBuf[m_inPos]);
    }

    static void MakeKeyEvent(INPUT_RECORD& ir, WORD vk, wchar_t ch, DWORD ctrlState = 0) {
        ir.EventType = KEY_EVENT;
        ir.Event.KeyEvent.bKeyDown = TRUE;
        ir.Event.KeyEvent.wRepeatCount = 1;
        ir.Event.KeyEvent.wVirtualKeyCode = vk;
        ir.Event.KeyEvent.wVirtualScanCode = 0;
        ir.Event.KeyEvent.uChar.UnicodeChar = ch;
        ir.Event.KeyEvent.dwControlKeyState = ctrlState;
    }

    // Map a CSI final-letter / tilde-code to a virtual key + control state.
    bool ParseCSI(INPUT_RECORD& ir) {
        // We've consumed "ESC [". Collect params until a final byte.
        std::string params;
        int b = ReadByte();
        // SGR mouse: ESC [ < ... M/m
        if (b == '<') {
            std::string mparams;
            int c = ReadByte();
            while (c != -1 && c != 'M' && c != 'm') { mparams += (char)c; c = ReadByte(); }
            ParseSgrMouse(ir, mparams, c == 'M');
            return true;
        }
        while (b != -1 && !(b >= 0x40 && b <= 0x7E)) { params += (char)b; b = ReadByte(); }
        if (b == -1) return false;
        char finalByte = (char)b;

        DWORD ctrl = 0;
        // params may be "1;5" style for modifiers; 2=Shift,4=Ctrl,5=Ctrl(+Shift)
        if (params.find("5") != std::string::npos || params.find("4") != std::string::npos) {
            ctrl |= LEFT_CTRL_PRESSED;
        }

        switch (finalByte) {
            case 'A': MakeKeyEvent(ir, VK_UP, 0, ctrl); return true;
            case 'B': MakeKeyEvent(ir, VK_DOWN, 0, ctrl); return true;
            case 'C': MakeKeyEvent(ir, VK_RIGHT, 0, ctrl); return true;
            case 'D': MakeKeyEvent(ir, VK_LEFT, 0, ctrl); return true;
            case 'H': MakeKeyEvent(ir, VK_HOME, 0, ctrl); return true;
            case 'F': MakeKeyEvent(ir, VK_END, 0, ctrl); return true;
            case 'Z': MakeKeyEvent(ir, VK_TAB, L'\t', SHIFT_PRESSED); return true;
            case '~': {
                int code = params.empty() ? 0 : atoi(params.c_str());
                switch (code) {
                    case 1: MakeKeyEvent(ir, VK_HOME, 0, ctrl); return true;
                    case 2: MakeKeyEvent(ir, VK_INSERT, 0, ctrl); return true;
                    case 3: MakeKeyEvent(ir, VK_DELETE, 0, ctrl); return true;
                    case 4: MakeKeyEvent(ir, VK_END, 0, ctrl); return true;
                    case 5: MakeKeyEvent(ir, VK_PRIOR, 0, ctrl); return true;  // PgUp
                    case 6: MakeKeyEvent(ir, VK_NEXT, 0, ctrl); return true;   // PgDn
                    case 7: MakeKeyEvent(ir, VK_HOME, 0, ctrl); return true;
                    case 8: MakeKeyEvent(ir, VK_END, 0, ctrl); return true;
                    case 11: MakeKeyEvent(ir, VK_F1, 0, ctrl); return true;
                    case 12: MakeKeyEvent(ir, VK_F2, 0, ctrl); return true;
                    case 13: MakeKeyEvent(ir, VK_F3, 0, ctrl); return true;
                    case 14: MakeKeyEvent(ir, VK_F4, 0, ctrl); return true;
                    case 15: MakeKeyEvent(ir, VK_F5, 0, ctrl); return true;
                    case 17: MakeKeyEvent(ir, VK_F6, 0, ctrl); return true;
                    case 18: MakeKeyEvent(ir, VK_F7, 0, ctrl); return true;
                    case 19: MakeKeyEvent(ir, VK_F8, 0, ctrl); return true;
                    case 20: MakeKeyEvent(ir, VK_F9, 0, ctrl); return true;
                    case 21: MakeKeyEvent(ir, VK_F10, 0, ctrl); return true;
                    case 23: MakeKeyEvent(ir, VK_F11, 0, ctrl); return true;
                    case 24: MakeKeyEvent(ir, VK_F12, 0, ctrl); return true;
                    default: return false;
                }
            }
            default: return false;
        }
    }

    // Parse SGR mouse params "b;x;y" into a MOUSE_EVENT INPUT_RECORD.
    void ParseSgrMouse(INPUT_RECORD& ir, const std::string& params, bool pressed) {
        int btn = 0, x = 0, y = 0;
        sscanf(params.c_str(), "%d;%d;%d", &btn, &x, &y);
        ir.EventType = MOUSE_EVENT;
        auto& me = ir.Event.MouseEvent;
        // Clamp to a sane console range so a malformed/hostile sequence can't
        // produce out-of-range coordinates (which would index the screen buffer).
        int cx = (std::max)(0, (std::min)(x - 1, 32766));
        int cy = (std::max)(0, (std::min)(y - 1, 32766));
        me.dwMousePosition.X = static_cast<SHORT>(cx);
        me.dwMousePosition.Y = static_cast<SHORT>(cy);
        me.dwEventFlags = 0;
        me.dwControlKeyState = 0;
        DWORD btnState = 0;
        if (btn & 64) { // wheel
            me.dwEventFlags = MOUSE_WHEELED;
            int delta = (btn & 1) ? -120 : 120; // 65=down,64=up
            btnState = (DWORD)(delta << 16);
        } else if (pressed) {
            if ((btn & 3) == 0) btnState |= FROM_LEFT_1ST_BUTTON_PRESSED;
            else if ((btn & 3) == 1) btnState |= FROM_LEFT_2ND_BUTTON_PRESSED;
            else if ((btn & 3) == 2) btnState |= RIGHTMOST_BUTTON_PRESSED;
        }
        me.dwButtonState = btnState;
    }

    // Decode the next VT input sequence into an INPUT_RECORD.
    // Returns true when a complete event was decoded.
    bool DecodeInputEvent(INPUT_RECORD& ir) {
        int b = ReadByte();
        if (b == -1) return false;

        if (b == 0x1B) { // ESC: could be a sequence or a lone Alt/Esc
            int n = PeekByte();
            if (n == '[') { ReadByte(); return ParseCSI(ir); }
            if (n == 'O') { // SS3 F-keys
                ReadByte();
                int f = ReadByte();
                switch (f) {
                    case 'P': MakeKeyEvent(ir, VK_F1, 0); return true;
                    case 'Q': MakeKeyEvent(ir, VK_F2, 0); return true;
                    case 'R': MakeKeyEvent(ir, VK_F3, 0); return true;
                    case 'S': MakeKeyEvent(ir, VK_F4, 0); return true;
                    default: return false;
                }
            }
            // Lone ESC.
            MakeKeyEvent(ir, VK_ESCAPE, 0);
            return true;
        }

        // Control characters -> Ctrl+letter combos.
        if (b >= 1 && b <= 26) {
            MakeKeyEvent(ir, (WORD)('A' + b - 1), (wchar_t)b, LEFT_CTRL_PRESSED);
            return true;
        }
        if (b == 13) { MakeKeyEvent(ir, VK_RETURN, L'\r'); return true; }
        if (b == 9)  { MakeKeyEvent(ir, VK_TAB, L'\t'); return true; }
        if (b == 8 || b == 127) { MakeKeyEvent(ir, VK_BACK, L'\b'); return true; }

        // UTF-8 text.
        wchar_t wc;
        if (b < 0x80) {
            wc = (wchar_t)b;
        } else {
            int extra = (b >= 0xF0) ? 3 : (b >= 0xE0) ? 2 : 1;
            uint32_t cp = b & ((1 << (7 - (extra + 1))) - 1);
            for (int k = 0; k < extra; ++k) {
                int cb = ReadByte();
                if (cb == -1) return false;
                cp = (cp << 6) | (uint32_t)(cb & 0x3F);
            }
            wc = (wchar_t)cp;
        }
        MakeKeyEvent(ir, 0, wc);
        return true;
    }

    // Block until an input event is available and decode it into ir.
    bool ReadInputEvent(INPUT_RECORD& ir) {
        if (m_useClassicConsoleInput) {
            DWORD read = 0;
            return ReadConsoleInputW(m_hStdIn, &ir, 1, &read) && read == 1;
        }

        while (true) {
            if (DecodeInputEvent(ir)) return true;
            // Wait for more input if we couldn't decode a full event.
            if (WaitForSingleObject(m_hStdIn, INFINITE) != WAIT_OBJECT_0) return false;
        }
    }

    // ========================================================================
    // VT/ANSI TERMINAL OUTPUT BACKEND
    // Renders the CHAR_INFO screen buffer as ANSI/VT escape sequences so the UI
    // works identically on a local console (with VT processing enabled) and over
    // SSH / headless PTY sessions, where the classic WriteConsoleOutputW API has
    // no meaning. Console attribute bitfields are translated to SGR color codes.
    // ========================================================================

    // Map a console color nibble (0-15) to an ANSI SGR base color code.
    // Console nibble order is BGR: bit0=Blue, bit1=Green, bit2=Red, bit3=Intensity.
    static int ConsoleColorToAnsiIndex(WORD colorNibble) {
        static const int kMap[16] = {
            0, // 0 black
            4, // 1 blue   -> ANSI blue
            2, // 2 green  -> ANSI green
            6, // 3 cyan   -> ANSI cyan
            1, // 4 red    -> ANSI red
            5, // 5 magenta-> ANSI magenta
            3, // 6 yellow -> ANSI yellow
            7, // 7 white(gray)
            0, // 8 bright black (handled via intensity)
            4, 2, 6, 1, 5, 3, 7
        };
        return kMap[colorNibble & 0x0F];
    }

    // Emit the SGR sequence for a console attribute into `out`.
    static void AppendAttrAsSGR(std::string& out, WORD attr) {
        WORD fg = attr & 0x0F;               // foreground nibble (bits 0-3)
        WORD bg = (attr >> 4) & 0x0F;        // background nibble (bits 4-7)
        bool fgIntense = (fg & FOREGROUND_INTENSITY) != 0;
        bool bgIntense = (bg & BACKGROUND_INTENSITY) != 0;

        out += "\x1b[0"; // reset
        if (fgIntense) out += ";1";
        if (bgIntense) out += ";4"; // underline as a cheap "bright bg" hint on limited terms
        char buf[16];
        // Foreground: 30-37 normal, 90-97 bright.
        int fgBase = ConsoleColorToAnsiIndex(fg);
        snprintf(buf, sizeof(buf), ";%d", (fgIntense ? 90 : 30) + fgBase);
        out += buf;
        // Background: 40-47 normal, 100-107 bright.
        int bgBase = ConsoleColorToAnsiIndex(bg);
        snprintf(buf, sizeof(buf), ";%d", (bgIntense ? 100 : 40) + bgBase);
        out += buf;
        out += "m";
    }

    // Append a wide char as UTF-8 to `out`.
    static void AppendUtf8(std::string& out, wchar_t wc) {
        char tmp[8];
        int n = WideCharToMultiByte(CP_UTF8, 0, &wc, 1, tmp, sizeof(tmp), nullptr, nullptr);
        if (n > 0) out.append(tmp, static_cast<size_t>(n));
    }

    void FlushBuffer() {
        // Compose the whole frame as one VT byte stream: home the cursor, then
        // walk rows emitting cursor moves, SGR color changes (only when the
        // attribute changes), and UTF-8 text. One write per frame avoids flicker.
        std::string frame;
        frame.reserve(static_cast<size_t>(m_width) * static_cast<size_t>(m_height) * 2 + 256);

        frame += "\x1b[H";       // cursor home
        frame += "\x1b[?25l";    // hide cursor during paint

        WORD lastAttr = 0xFFFF;  // force an initial SGR emit
        char esc[32];
        for (int y = 0; y < m_height; ++y) {
            // Position at start of row (1-based row/col).
            snprintf(esc, sizeof(esc), "\x1b[%d;1H", y + 1);
            frame += esc;
            for (int x = 0; x < m_width; ++x) {
                const CHAR_INFO& cell = m_screenBuffer[y * m_width + x];
                if (cell.Attributes != lastAttr) {
                    AppendAttrAsSGR(frame, cell.Attributes);
                    lastAttr = cell.Attributes;
                }
                wchar_t ch = cell.Char.UnicodeChar;
                AppendUtf8(frame, ch == 0 ? L' ' : ch);
            }
        }
        frame += "\x1b[0m";      // reset attributes
        frame += "\x1b[?25h";    // show cursor

        DWORD written = 0;
        WriteFile(m_hStdOut, frame.data(), static_cast<DWORD>(frame.size()), &written, nullptr);
    }

    // ========================================================================
    // UI RENDERING
    // ========================================================================

    void RenderScreen() {
        ClearBuffer(m_palette.DesktopAttr);

        RenderMenuBar();
        RenderEditorWindows();
        RenderStatusBar();

        FlushBuffer();
    }

    void RenderMenuBar() {
        // Draw Top Menu Bar
        for (int x = 0; x < m_width; ++x) {
            PutChar(x, 0, L' ', m_palette.MenuAttr);
        }

        std::string menuStr = " File  Edit  Search  View  Compile  Git  Debug  Options  Window  Help";
        DrawString(1, 0, menuStr, m_palette.MenuAttr);

        // Highlight every full menu segment so the visible block and click target match.
        TopMenuAction actions[] = {
            TopMenuAction::File,
            TopMenuAction::Edit,
            TopMenuAction::Search,
            TopMenuAction::Run,
            TopMenuAction::Compile,
            TopMenuAction::Git,
            TopMenuAction::Debug,
            TopMenuAction::Options,
            TopMenuAction::Window,
            TopMenuAction::Help
        };

        for (TopMenuAction action : actions) {
            int regionStartX = 0;
            int regionEndX = 0;
            if (!TryGetTopMenuRegionX(action, regionStartX, regionEndX)) {
                continue;
            }

            for (int x = regionStartX; x <= regionEndX && x < m_width; ++x) {
                PutChar(x, 0, L' ', m_palette.MenuHotkeyAttr);
            }

            DrawString(regionStartX, 0, menuStr.substr(regionStartX - 1, regionEndX - regionStartX + 1), m_palette.MenuHotkeyAttr);
        }

        // Pressed/open menu segment keeps the same menu theme colors.
        if (m_pressedTopMenuAction != TopMenuAction::None) {
            int regionStartX = 0;
            int regionEndX = 0;
            if (TryGetTopMenuRegionX(m_pressedTopMenuAction, regionStartX, regionEndX)) {
                WORD pressedAttr = m_palette.MenuHotkeyAttr;
                for (int x = regionStartX; x <= regionEndX && x < m_width; ++x) {
                    PutChar(x, 0, L' ', pressedAttr);
                }
                DrawString(regionStartX, 0, menuStr.substr(regionStartX - 1, regionEndX - regionStartX + 1), pressedAttr);
            }
        }
    }

    bool TryGetTopMenuRegionX(TopMenuAction action, int& startX, int& endX) const {
        const std::string menuStr = " File  Edit  Search  View  Compile  Git  Debug  Options  Window  Help";

        struct MenuToken {
            TopMenuAction Action;
            const char* Token;
        };

        const MenuToken tokens[] = {
            {TopMenuAction::File, "File"},
            {TopMenuAction::Edit, "Edit"},
            {TopMenuAction::Search, "Search"},
            {TopMenuAction::Run, "View"},
            {TopMenuAction::Compile, "Compile"},
            {TopMenuAction::Git, "Git"},
            {TopMenuAction::Debug, "Debug"},
            {TopMenuAction::Options, "Options"},
            {TopMenuAction::Window, "Window"},
            {TopMenuAction::Help, "Help"}
        };

        size_t scanPos = 0;
        for (const auto& token : tokens) {
            size_t idx = menuStr.find(token.Token, scanPos);
            if (idx == std::string::npos) break;

            int left = static_cast<int>(idx);
            int right = static_cast<int>(idx + std::strlen(token.Token) - 1);

            if (left > 0 && menuStr[left - 1] == ' ') {
                --left;
            }
            while (right + 1 < static_cast<int>(menuStr.size()) && menuStr[right + 1] == ' ') {
                ++right;
            }

            if (token.Action == action) {
                startX = 1 + left;
                endX = 1 + right;
                return true;
            }

            scanPos = idx + std::strlen(token.Token);
        }

        return false;
    }

    void EnsureDocumentInvariants(const std::shared_ptr<Document>& doc) {
        if (!doc) return;
        if (doc->Lines.empty()) {
            doc->Lines.push_back("");
        }

        if (doc->CursorY < 0) doc->CursorY = 0;
        if (doc->CursorY >= static_cast<int>(doc->Lines.size())) {
            doc->CursorY = static_cast<int>(doc->Lines.size()) - 1;
        }

        if (doc->CursorX < 0) doc->CursorX = 0;
        int lineLen = static_cast<int>(doc->Lines[doc->CursorY].length());
        if (doc->CursorX > lineLen) doc->CursorX = lineLen;
        // Snap CursorX back to a UTF-8 codepoint boundary so rendering and
        // editing never split a multi-byte sequence.
        const std::string& curLine = doc->Lines[doc->CursorY];
        while (doc->CursorX > 0 && doc->CursorX < lineLen &&
               (static_cast<unsigned char>(curLine[doc->CursorX]) & 0xC0) == 0x80) {
            doc->CursorX--;
        }
    }

    void SetStatus(const std::string& text) {
        m_statusMessage = text;
        m_needsRender = true;
    }

    static bool SetSystemClipboardText(const std::string& text) {
        int wideLength = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
        if (wideLength < 0 || !OpenClipboard(nullptr)) return false;
        EmptyClipboard();
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, static_cast<size_t>(wideLength + 1) * sizeof(wchar_t));
        if (!memory) {
            CloseClipboard();
            return false;
        }
        auto* buffer = static_cast<wchar_t*>(GlobalLock(memory));
        if (!buffer) {
            GlobalFree(memory);
            CloseClipboard();
            return false;
        }
        if (wideLength > 0) {
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), buffer, wideLength);
        }
        buffer[wideLength] = L'\0';
        GlobalUnlock(memory);
        if (!SetClipboardData(CF_UNICODETEXT, memory)) {
            GlobalFree(memory);
            CloseClipboard();
            return false;
        }
        CloseClipboard();
        return true;
    }

    static bool GetSystemClipboardText(std::string& text) {
        text.clear();
        if (!OpenClipboard(nullptr)) return false;
        HANDLE handle = GetClipboardData(CF_UNICODETEXT);
        if (!handle) {
            CloseClipboard();
            return false;
        }
        const auto* wideText = static_cast<const wchar_t*>(GlobalLock(handle));
        if (!wideText) {
            CloseClipboard();
            return false;
        }
        int utf8Length = WideCharToMultiByte(CP_UTF8, 0, wideText, -1, nullptr, 0, nullptr, nullptr);
        if (utf8Length > 1) {
            text.resize(static_cast<size_t>(utf8Length));
            WideCharToMultiByte(CP_UTF8, 0, wideText, -1, text.data(), utf8Length, nullptr, nullptr);
            text.resize(static_cast<size_t>(utf8Length - 1));
        }
        GlobalUnlock(handle);
        CloseClipboard();
        return true;
    }

    void UndoActiveDocument() {
        if (m_documents.empty()) return;
        auto doc = m_documents[m_activeDocIndex];
        if (doc && doc->Undo()) {
            EnsureDocumentInvariants(doc);
            SetStatus("Undo.");
        } else {
            SetStatus("Nothing to undo.");
        }
    }

    void RedoActiveDocument() {
        if (m_documents.empty()) return;
        auto doc = m_documents[m_activeDocIndex];
        if (doc && doc->Redo()) {
            EnsureDocumentInvariants(doc);
            SetStatus("Redo.");
        } else {
            SetStatus("Nothing to redo.");
        }
    }

    static bool IsCtrlPressed(DWORD state) {
        return (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
    }

    static bool IsAltPressed(DWORD state) {
        return (state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
    }

    static bool IsShiftPressed(DWORD state) {
        return (state & SHIFT_PRESSED) != 0;
    }

    bool HandleMenuAccelerator(WORD vk, DWORD state) {
        const bool ctrl = IsCtrlPressed(state);
        const bool alt = IsAltPressed(state);
        const bool shift = IsShiftPressed(state);

        if (alt && vk == 'X') {
            ExecuteMenuCommand(MenuCommand::FileExit);
            return true;
        }

        if (vk == VK_F1) {
            ExecuteMenuCommand(MenuCommand::HelpTopics);
            return true;
        }
        if (vk == VK_F4) {
            ExecuteMenuCommand(MenuCommand::HelpDiagnostics);
            return true;
        }
        if (vk == VK_F8) {
            ExecuteMenuCommand(MenuCommand::RunTerminal);
            return true;
        }
        if (vk == VK_F9) {
            if (alt) ExecuteMenuCommand(MenuCommand::BuildDetect);
            else ExecuteMenuCommand(MenuCommand::CompileBuild);
            return true;
        }
        if (vk == VK_F10) {
            ExecuteMenuCommand(MenuCommand::CompileRun);
            return true;
        }
        if (ctrl && !shift && !alt) {
            switch (vk) {
            case 'N': ExecuteMenuCommand(MenuCommand::FileNew); return true;
            case 'O': ExecuteMenuCommand(MenuCommand::FileOpen); return true;
            case 'S': ExecuteMenuCommand(MenuCommand::FileSave); return true;
            case 'W': ExecuteMenuCommand(MenuCommand::FileClose); return true;
            case 'Z': ExecuteMenuCommand(MenuCommand::EditUndo); return true;
            case 'Y': ExecuteMenuCommand(MenuCommand::EditRedo); return true;
            case 'F': ExecuteMenuCommand(MenuCommand::SearchFind); return true;
            case 'G': ExecuteMenuCommand(MenuCommand::SearchFindNext); return true;
            case 'H': ExecuteMenuCommand(MenuCommand::SearchReplace); return true;
            case 'X': ExecuteMenuCommand(MenuCommand::EditCut); return true;
            case 'V': ExecuteMenuCommand(MenuCommand::EditPaste); return true;
            case 'A': ExecuteMenuCommand(MenuCommand::EditSelectAll); return true;
            case 'B': ExecuteMenuCommand(MenuCommand::CompileBuild); return true;
            case 'R': ExecuteMenuCommand(MenuCommand::CompileRun); return true;
            case 'T': ExecuteMenuCommand(MenuCommand::CompileTest); return true;
            case 'L': ExecuteMenuCommand(MenuCommand::OptionsRunLinker); return true;
            // Window switch: Ctrl+E is the primary combo (Ctrl+Tab is captured by
            // Windows Terminal's own tab switching, so keep it only as a fallback
            // for classic conhost where it still reaches us).
            case 'E': ExecuteMenuCommand(MenuCommand::WindowNext); return true;
            case VK_TAB: ExecuteMenuCommand(MenuCommand::WindowNext); return true;
            default: break;
            }
        }

        if (ctrl && shift && !alt) {
            switch (vk) {
            case 'S': ExecuteMenuCommand(MenuCommand::FileSaveAs); return true;
            case 'C': ExecuteMenuCommand(MenuCommand::EditCopy); return true;
            case 'D': ExecuteMenuCommand(MenuCommand::DebugDetect); return true;
            case 'Y': ExecuteMenuCommand(MenuCommand::DebugViewOutput); return true;
            case 'O': ExecuteMenuCommand(MenuCommand::ViewCompilerOutput); return true;
            case 'G': ExecuteMenuCommand(MenuCommand::SearchFindPrevious); return true;
            case 'I': ExecuteMenuCommand(MenuCommand::EditInsertLine); return true;
            case 'U': ExecuteMenuCommand(MenuCommand::EditDuplicateLine); return true;
            case VK_BACK: ExecuteMenuCommand(MenuCommand::EditDeleteLine); return true;
            default: break;
            }
        }

        if (ctrl && alt && !shift) {
            switch (vk) {
            case 'N': ExecuteMenuCommand(MenuCommand::WindowNew); return true;
            case 'T': ExecuteMenuCommand(MenuCommand::OptionsNextTheme); return true;
            case 'P': ExecuteMenuCommand(MenuCommand::OptionsAddCompilerPath); return true;
            case 'D': ExecuteMenuCommand(MenuCommand::DebugRunActive); return true;
            case 'A': ExecuteMenuCommand(MenuCommand::HelpAbout); return true;
            case 'L': ExecuteMenuCommand(MenuCommand::WindowLayoutToggle); return true;
            default: break;
            }
        }

        return false;
    }

    void NewDocument() {
        int id = static_cast<int>(m_documents.size()) + 1;
        auto doc = std::make_shared<Document>(id, "");
        doc->Lang = SupportedLanguage::Text;
        m_documents.push_back(doc);
        m_activeDocIndex = static_cast<int>(m_documents.size()) - 1;
        m_needsRender = true;
    }

    void SaveActiveDocument() {
        if (m_documents.empty()) return;
        auto doc = m_documents[m_activeDocIndex];
        if (!doc) return;

        if (doc->FilePath.empty()) {
            std::string selectedPath;
            if (!ShowPathNavigatorDialog("Save File", false, false, selectedPath)) {
                SetStatus("Save cancelled.");
                return;
            }

            std::string normalizedPath;
            if (!NormalizePathString(selectedPath, normalizedPath)) {
                SetStatus("Save failed: invalid file path.");
                return;
            }

            doc->FilePath = normalizedPath;
            doc->Title = fs::path(normalizedPath).filename().string();
            doc->Lang = m_langRegistry.DetectLanguage(doc->FilePath);
            RememberRecentFile(doc->FilePath);
        }
        doc->SaveFile();
        m_selectAllActive = false;
        SetStatus("Saved: " + doc->Title);
    }

    void SaveActiveDocumentAs(const std::string& path) {
        if (m_documents.empty()) return;
        auto doc = m_documents[m_activeDocIndex];
        if (!doc) return;

        std::string selectedPath = path;
        if (selectedPath.empty()) {
            if (!ShowPathNavigatorDialog("Save File As", false, false, selectedPath)) {
                SetStatus("Save As cancelled.");
                return;
            }
        }

        std::string normalizedPath;
        if (!NormalizePathString(selectedPath, normalizedPath)) {
            SetStatus("Save As failed: invalid file path.");
            return;
        }

        doc->FilePath = normalizedPath;
        doc->Title = fs::path(normalizedPath).filename().string();
        doc->Lang = m_langRegistry.DetectLanguage(doc->FilePath);
        RememberRecentFile(doc->FilePath);
        doc->SaveFile();
        m_selectAllActive = false;
        SetStatus("Saved As: " + doc->Title);
        m_needsRender = true;
    }

    void CycleTheme() {
        int next = (static_cast<int>(m_currentTheme) + 1) % (static_cast<int>(Theme_RetroAmber) + 1);
        m_currentTheme = static_cast<ThemeScheme>(next);
        m_palette = ThemeManager::GetTheme(m_currentTheme);
        m_needsRender = true;
    }

    // Compute the [topY, botY] frame for a document window under the current layout.
    // Returns false if the document is not visible in this layout (e.g. inactive in
    // Maximized/Tabs mode). In Tabs mode the top row is reserved for the tab bar.
    bool GetDocWindowFrame(int docIndex, int& topY, int& botY) const {
        int windowTop = 1;
        int windowBottom = m_height - 2;
        int totalHeight = windowBottom - windowTop;
        int numDocs = static_cast<int>(m_documents.size());
        if (numDocs <= 0) return false;

        switch (m_windowLayout) {
        case WindowLayout::Maximized:
            if (docIndex != m_activeDocIndex) return false;
            topY = windowTop;
            botY = windowBottom;
            return true;
        case WindowLayout::Tabs:
            if (docIndex != m_activeDocIndex) return false;
            topY = windowTop + 1; // row 1 is the tab strip
            botY = windowBottom;
            return true;
        case WindowLayout::Split:
        default: {
            int docHeight = totalHeight / numDocs;
            topY = windowTop + docIndex * docHeight;
            botY = (docIndex == numDocs - 1) ? windowBottom : topY + docHeight - 1;
            return true;
        }
        }
    }

    int GetDocumentViewHeight(int docIndex) const {
        int topY = 0, botY = 0;
        if (!GetDocWindowFrame(docIndex, topY, botY)) return 1;
        return (std::max)(1, botY - topY - 1);
    }

    void EnsureCursorVisible(const std::shared_ptr<Document>& doc, int docIndex) {
        if (!doc) return;

        int viewHeight = GetDocumentViewHeight(docIndex);
        if (doc->CursorY < doc->ScrollY) {
            doc->ScrollY = doc->CursorY;
        } else if (doc->CursorY >= doc->ScrollY + viewHeight) {
            doc->ScrollY = doc->CursorY - viewHeight + 1;
        }

        int maxScroll = (std::max)(0, static_cast<int>(doc->Lines.size()) - 1);
        doc->ScrollY = (std::max)(0, (std::min)(doc->ScrollY, maxScroll));

        // Horizontal scroll: keep the cursor's visual column within the text
        // viewport. ScrollX and the cursor column are DISPLAY CELLS (not bytes),
        // so wide/multi-byte characters count by their on-screen width.
        const int lineNumWidth = 5;
        int viewWidth = m_width - lineNumWidth - 3;
        if (viewWidth < 1) viewWidth = 1;

        const std::string& line = doc->Lines[doc->CursorY];
        int lineLen = static_cast<int>(line.size());
        if (doc->CursorX > lineLen) doc->CursorX = lineLen;

        int cursorCol = ByteOffsetToDisplayCol(line, doc->CursorX);
        if (cursorCol < doc->ScrollX) {
            doc->ScrollX = cursorCol;
        } else if (cursorCol >= doc->ScrollX + viewWidth) {
            doc->ScrollX = cursorCol - viewWidth + 1;
        }
        if (doc->ScrollX < 0) doc->ScrollX = 0;
    }

    void OpenDocumentFromPrompt() {
        std::string path;
        if (!ShowPathNavigatorDialog("Open File", true, true, path)) {
            SetStatus("Open cancelled.");
            return;
        }

        if (!path.empty()) {
            int id = static_cast<int>(m_documents.size()) + 1;
            auto doc = std::make_shared<Document>(id, path);
            doc->Lang = m_langRegistry.DetectLanguage(path);
            m_documents.push_back(doc);
            m_activeDocIndex = static_cast<int>(m_documents.size()) - 1;
            RememberRecentFile(path);
            SetStatus("Opened: " + doc->Title);
        } else {
            SetStatus("Open cancelled.");
        }
    }

    bool ShowPathNavigatorDialog(const std::string& title, bool mustExistFile, bool openMode, std::string& ioPath, bool selectDirectory = false) {
        fs::path currentDir;
        std::string fileName;

        if (!ioPath.empty()) {
            fs::path incoming = fs::path(ioPath);
            if (fs::exists(incoming)) {
                if (fs::is_directory(incoming)) {
                    currentDir = fs::absolute(incoming);
                } else {
                    currentDir = fs::absolute(incoming.parent_path());
                    fileName = incoming.filename().string();
                }
            } else {
                currentDir = fs::absolute(incoming.parent_path().empty() ? fs::current_path() : incoming.parent_path());
                fileName = incoming.filename().string();
            }
        } else {
            currentDir = fs::current_path();
        }

        int dWidth = 0;
        int dHeight = 0;
        int startX = 0;
        int startY = 0;
        int listTop = 0;
        int listBottom = 0;
        int visibleRows = 0;

        auto recomputeLayout = [&]() {
            dWidth = (std::min)(m_width - 4, 100);
            dHeight = (std::min)(m_height - 4, 26);
            dWidth = (std::max)(dWidth, 52);
            dHeight = (std::max)(dHeight, 18);
            startX = (m_width - dWidth) / 2;
            startY = (m_height - dHeight) / 2;
            listTop = startY + 4;
            listBottom = startY + dHeight - 7;
            visibleRows = (std::max)(3, listBottom - listTop + 1);
        };

        recomputeLayout();
        int selected = 0;
        int scroll = 0;
        bool listFocus = true;
        std::string message;

        auto loadEntries = [&](std::vector<fs::directory_entry>& entries) {
            entries.clear();
            try {
                for (const auto& de : fs::directory_iterator(currentDir)) {
                    entries.push_back(de);
                }
                std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
                    bool aDir = a.is_directory();
                    bool bDir = b.is_directory();
                    if (aDir != bDir) return aDir > bDir;
                    return a.path().filename().string() < b.path().filename().string();
                });
            } catch (const std::exception& ex) {
                message = std::string("Unable to read folder: ") + ex.what();
            } catch (...) {
                message = "Unable to read folder.";
            }
        };

        std::vector<fs::directory_entry> entries;
        loadEntries(entries);
        bool prevCursorVisible = SetCursorVisible(false);
        bool dialogDirty = true;

        RenderScreen();
        m_needsRender = false;
        std::vector<CHAR_INFO> dialogBaseBuffer = m_screenBuffer;

        while (true) {
            if (dialogDirty) {
                if (dialogBaseBuffer.size() == m_screenBuffer.size()) {
                    m_screenBuffer = dialogBaseBuffer;
                }

                WORD dAttr = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
                WORD borderAttr = dAttr | FOREGROUND_BLUE;

                PutChar(startX, startY, '+', borderAttr);
                PutChar(startX + dWidth - 1, startY, '+', borderAttr);
                PutChar(startX, startY + dHeight - 1, '+', borderAttr);
                PutChar(startX + dWidth - 1, startY + dHeight - 1, '+', borderAttr);
                for (int x = startX + 1; x < startX + dWidth - 1; ++x) {
                    PutChar(x, startY, '-', borderAttr);
                    PutChar(x, startY + dHeight - 1, '-', borderAttr);
                }
                for (int y = startY + 1; y < startY + dHeight - 1; ++y) {
                    PutChar(startX, y, '|', borderAttr);
                    PutChar(startX + dWidth - 1, y, '|', borderAttr);
                    for (int x = startX + 1; x < startX + dWidth - 1; ++x) {
                        PutChar(x, y, ' ', dAttr);
                    }
                }

                DrawString(startX + 2, startY + 1, title, dAttr | FOREGROUND_BLUE);

                std::string dirLine = "Dir: " + currentDir.string();
                int maxLineLen = dWidth - 4;
                if ((int)dirLine.size() > maxLineLen) {
                    dirLine = "..." + dirLine.substr(dirLine.size() - (maxLineLen - 3));
                }
                DrawString(startX + 2, startY + 2, dirLine, dAttr);

                if (selectDirectory) {
                    DrawString(startX + 2, startY + 3, "Use wheel/Up/Down, Enter(open), F4(select folder), Tab(field), Esc(cancel)", dAttr);
                } else {
                    DrawString(startX + 2, startY + 3, "Use wheel/Up/Down Enter, Backspace(parent), Tab(field), Esc(cancel)", dAttr);
                }

                int itemAreaWidth = dWidth - 4;
                if (selected < scroll) scroll = selected;
                if (selected >= scroll + visibleRows) scroll = selected - visibleRows + 1;
                if (scroll < 0) scroll = 0;

                for (int row = 0; row < visibleRows; ++row) {
                    int idx = scroll + row;
                    int y = listTop + row;
                    std::string line;
                    if (idx == 0) {
                        line = "[DIR] ..";
                    } else if (idx - 1 < static_cast<int>(entries.size())) {
                        const auto& de = entries[idx - 1];
                        std::string name = de.path().filename().string();
                        line = (de.is_directory() ? "[DIR] " : "[FILE] ") + name;
                    }

                    if ((int)line.size() > itemAreaWidth) {
                        line = line.substr(0, itemAreaWidth - 3) + "...";
                    }
                    while ((int)line.size() < itemAreaWidth) line += ' ';

                    WORD lineAttr = dAttr;
                    if (idx == selected) {
                        lineAttr = static_cast<WORD>(BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY);
                    }
                    DrawString(startX + 2, y, line, lineAttr);
                }

                std::string nameLabel = selectDirectory ? "Folder:" : (openMode ? "Open:" : "Save:");
                DrawString(startX + 2, startY + dHeight - 4, nameLabel, dAttr | FOREGROUND_BLUE);

                std::string shownName = fileName;
                int inputMax = dWidth - 12;
                if ((int)shownName.size() > inputMax) {
                    shownName = shownName.substr(shownName.size() - inputMax);
                }
                while ((int)shownName.size() < inputMax) shownName += ' ';
                DrawString(startX + 10, startY + dHeight - 4, shownName, listFocus ? dAttr : static_cast<WORD>(dAttr | FOREGROUND_BLUE));

                std::string statusLine;
                if (!message.empty()) {
                    statusLine = message;
                } else if (selectDirectory) {
                    statusLine = "F4 selects current folder, Enter opens highlighted folder.";
                } else {
                    statusLine = "Enter file name or choose one from list.";
                }
                if ((int)statusLine.size() > maxLineLen) statusLine = statusLine.substr(0, maxLineLen);
                DrawString(startX + 2, startY + dHeight - 2, statusLine, dAttr);

                if (!listFocus) {
                    size_t lastNonSpace = shownName.find_last_not_of(' ');
                    int visibleLen = (lastNonSpace == std::string::npos) ? 0 : static_cast<int>(lastNonSpace + 1);
                    int cursorX = startX + 10 + visibleLen;
                    if (cursorX >= startX + dWidth - 2) cursorX = startX + dWidth - 3;
                    SetCursorVisible(true);
                    SetCursorPositionVT(cursorX, startY + dHeight - 4);
                } else {
                    SetCursorVisible(false);
                }

                FlushBuffer();
                dialogDirty = false;
            }

            INPUT_RECORD ir;
            DWORD read;
            ReadInputEvent(ir);

            if (ir.EventType == WINDOW_BUFFER_SIZE_EVENT) {
                UpdateBufferSize();
                recomputeLayout();
                RenderScreen();
                m_needsRender = false;
                dialogBaseBuffer = m_screenBuffer;
                dialogDirty = true;
                continue;
            }

            if (ir.EventType == MOUSE_EVENT) {
                COORD pos = ir.Event.MouseEvent.dwMousePosition;
                DWORD btn = ir.Event.MouseEvent.dwButtonState;
                DWORD flags = ir.Event.MouseEvent.dwEventFlags;

                if (flags == MOUSE_WHEELED) {
                    int wheel = static_cast<SHORT>(HIWORD(btn));
                    int totalItems = static_cast<int>(entries.size()) + 1;
                    int delta = (wheel > 0) ? -1 : 1;
                    selected = (std::max)(0, (std::min)(selected + delta, totalItems - 1));
                    dialogDirty = true;
                    continue;
                }

                if (flags == MOUSE_MOVED) {
                    if (pos.X >= startX + 2 && pos.X <= startX + dWidth - 3 && pos.Y >= listTop && pos.Y <= listBottom) {
                        int idx = scroll + (pos.Y - listTop);
                        int totalItems = static_cast<int>(entries.size()) + 1;
                        if (idx >= 0 && idx < totalItems && idx != selected) {
                            selected = idx;
                            dialogDirty = true;
                        }
                    }
                    continue;
                }

                if ((btn & FROM_LEFT_1ST_BUTTON_PRESSED) != 0 && flags == 0) {
                    if (pos.X >= startX + 2 && pos.X <= startX + dWidth - 3 && pos.Y >= listTop && pos.Y <= listBottom) {
                        int idx = scroll + (pos.Y - listTop);
                        int totalItems = static_cast<int>(entries.size()) + 1;
                        if (idx >= 0 && idx < totalItems) {
                            selected = idx;
                            dialogDirty = true;
                        }
                    } else if (!selectDirectory && pos.Y == startY + dHeight - 4) {
                        listFocus = false;
                        dialogDirty = true;
                    }
                    continue;
                }
            }

            if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) {
                continue;
            }

            WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
            DWORD state = ir.Event.KeyEvent.dwControlKeyState;
            wchar_t wch = ir.Event.KeyEvent.uChar.UnicodeChar;
            message.clear();

            if (vk == VK_ESCAPE) {
                SetCursorVisible(prevCursorVisible);
                return false;
            }

            if (selectDirectory && vk == VK_F4) {
                ioPath = fs::absolute(currentDir).string();
                SetCursorVisible(prevCursorVisible);
                return true;
            }

            if (vk == VK_TAB && !selectDirectory) {
                listFocus = !listFocus;
                dialogDirty = true;
                continue;
            }

            if (listFocus || selectDirectory) {
                int totalItems = static_cast<int>(entries.size()) + 1;
                if (vk == VK_UP) {
                    selected = (selected - 1 + totalItems) % totalItems;
                    dialogDirty = true;
                    continue;
                }
                if (vk == VK_DOWN) {
                    selected = (selected + 1) % totalItems;
                    dialogDirty = true;
                    continue;
                }
                if (vk == VK_BACK) {
                    if (currentDir.has_parent_path()) {
                        currentDir = currentDir.parent_path();
                        selected = 0;
                        scroll = 0;
                        loadEntries(entries);
                        dialogDirty = true;
                    }
                    continue;
                }
                if (vk == VK_RETURN) {
                    if (selectDirectory && (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))) {
                        ioPath = fs::absolute(currentDir).string();
                        SetCursorVisible(prevCursorVisible);
                        return true;
                    }

                    if (selected == 0) {
                        if (currentDir.has_parent_path()) {
                            currentDir = currentDir.parent_path();
                            selected = 0;
                            scroll = 0;
                            loadEntries(entries);
                            dialogDirty = true;
                        }
                        continue;
                    }

                    const auto& chosen = entries[selected - 1];
                    if (chosen.is_directory()) {
                        currentDir = chosen.path();
                        selected = 0;
                        scroll = 0;
                        loadEntries(entries);
                        dialogDirty = true;
                        continue;
                    }

                    if (selectDirectory) {
                        message = "Select a folder or press F4 for current folder.";
                        dialogDirty = true;
                        continue;
                    }

                    fileName = chosen.path().filename().string();
                    if (openMode) {
                        ioPath = fs::absolute(currentDir / fileName).string();
                        SetCursorVisible(prevCursorVisible);
                        return true;
                    }
                    listFocus = false;
                    dialogDirty = true;
                    continue;
                }
            } else {
                if (vk == VK_BACK) {
                    if (!fileName.empty()) PopLastUtf8Codepoint(fileName);
                    dialogDirty = true;
                    continue;
                }
                if (vk == VK_RETURN) {
                    if (fileName.empty()) {
                        message = "Please provide a file name.";
                        dialogDirty = true;
                        continue;
                    }
                    fs::path candidate = fs::absolute(currentDir / fileName);
                    if (mustExistFile && !fs::exists(candidate)) {
                        message = "File not found.";
                        dialogDirty = true;
                        continue;
                    }
                    if (mustExistFile && fs::is_directory(candidate)) {
                        message = "Selection is a folder.";
                        dialogDirty = true;
                        continue;
                    }
                    ioPath = candidate.string();
                    SetCursorVisible(prevCursorVisible);
                    return true;
                }
                std::string chUtf8 = Utf8FromWideChar(wch);
                if (!chUtf8.empty() && chUtf8.find_first_of("\"<>|:*?") == std::string::npos) {
                    fileName += chUtf8;
                    dialogDirty = true;
                    continue;
                }
            }
        }
    }

    bool ShowInputDialog(const std::string& title, const std::string& prompt, std::string& value) {
        value.clear();

        int dWidth = (std::max)(46, static_cast<int>(prompt.size()) + 8);
        dWidth = (std::min)(dWidth, m_width - 4);
        int dHeight = 7;
        int startX = (m_width - dWidth) / 2;
        int startY = (m_height - dHeight) / 2;

        bool prevCursorVisible = SetCursorVisible(true);
        bool dialogDirty = true;
        RenderScreen();
        m_needsRender = false;
        std::vector<CHAR_INFO> dialogBaseBuffer = m_screenBuffer;

        while (true) {
            if (dialogDirty) {
                if (dialogBaseBuffer.size() == m_screenBuffer.size()) {
                    m_screenBuffer = dialogBaseBuffer;
                }

                WORD dAttr = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
                WORD borderAttr = dAttr | FOREGROUND_BLUE;

                PutChar(startX, startY, '+', borderAttr);
                PutChar(startX + dWidth - 1, startY, '+', borderAttr);
                PutChar(startX, startY + dHeight - 1, '+', borderAttr);
                PutChar(startX + dWidth - 1, startY + dHeight - 1, '+', borderAttr);
                for (int x = startX + 1; x < startX + dWidth - 1; ++x) {
                    PutChar(x, startY, '-', borderAttr);
                    PutChar(x, startY + dHeight - 1, '-', borderAttr);
                }
                for (int y = startY + 1; y < startY + dHeight - 1; ++y) {
                    PutChar(startX, y, '|', borderAttr);
                    PutChar(startX + dWidth - 1, y, '|', borderAttr);
                    for (int x = startX + 1; x < startX + dWidth - 1; ++x) {
                        PutChar(x, y, ' ', dAttr);
                    }
                }

                DrawString(startX + 2, startY + 1, title, dAttr | FOREGROUND_BLUE);
                DrawString(startX + 2, startY + 2, prompt, dAttr);

                std::string shown = value;
                int maxInputLen = dWidth - 6;
                if ((int)shown.size() > maxInputLen) {
                    shown = shown.substr(shown.size() - maxInputLen);
                }
                DrawString(startX + 2, startY + 4, shown, dAttr);

                int cursorX = startX + 2 + static_cast<int>(shown.size());
                int cursorY = startY + 4;
                if (cursorX >= startX + dWidth - 2) cursorX = startX + dWidth - 3;
                FlushBuffer();
                SetCursorPositionVT(cursorX, cursorY);
                dialogDirty = false;
            }

            INPUT_RECORD ir;
            DWORD read;
            ReadInputEvent(ir);

            if (ir.EventType == WINDOW_BUFFER_SIZE_EVENT) {
                UpdateBufferSize();
                dWidth = (std::max)(46, static_cast<int>(prompt.size()) + 8);
                dWidth = (std::min)(dWidth, m_width - 4);
                dHeight = 7;
                startX = (m_width - dWidth) / 2;
                startY = (m_height - dHeight) / 2;
                RenderScreen();
                m_needsRender = false;
                dialogBaseBuffer = m_screenBuffer;
                dialogDirty = true;
                continue;
            }

            if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown) {
                continue;
            }

            WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
            wchar_t wch = ir.Event.KeyEvent.uChar.UnicodeChar;

            if (vk == VK_ESCAPE) {
                SetCursorVisible(prevCursorVisible);
                return false;
            }
            if (vk == VK_RETURN) {
                SetCursorVisible(prevCursorVisible);
                return true;
            }
            if (vk == VK_BACK) {
                if (!value.empty()) PopLastUtf8Codepoint(value);
                dialogDirty = true;
                continue;
            }
            std::string chUtf8 = Utf8FromWideChar(wch);
            if (!chUtf8.empty()) {
                value += chUtf8;
                dialogDirty = true;
            }
        }
    }

    std::string PromptLine(const std::string& prompt, const std::string& title = "Input") {
        std::string value;
        if (!ShowInputDialog(title, prompt, value)) {
            return "";
        }
        return value;
    }

    static void ReplaceAllTokens(std::string& text, const std::string& token, const std::string& value) {
        size_t pos = 0;
        while ((pos = text.find(token, pos)) != std::string::npos) {
            text.replace(pos, token.length(), value);
            pos += value.length();
        }
    }

    static std::string TrimWhitespace(const std::string& input) {
        size_t start = 0;
        while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start]))) {
            ++start;
        }

        size_t end = input.size();
        while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1]))) {
            --end;
        }

        return input.substr(start, end - start);
    }

    static std::string StripWrappingQuotes(std::string input) {
        input = TrimWhitespace(input);
        while (input.size() >= 2) {
            char first = input.front();
            char last = input.back();
            if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
                input = TrimWhitespace(input.substr(1, input.size() - 2));
                continue;
            }
            break;
        }
        return input;
    }

    static bool NormalizePathString(const std::string& rawPath, std::string& normalizedOut) {
        std::string cleaned = StripWrappingQuotes(rawPath);
        if (cleaned.empty()) {
            normalizedOut.clear();
            return false;
        }

        try {
            fs::path p = fs::absolute(fs::path(cleaned)).lexically_normal();
            normalizedOut = p.string();
            return !normalizedOut.empty();
        } catch (const std::exception&) {
            normalizedOut.clear();
            return false;
        } catch (...) {
            normalizedOut.clear();
            return false;
        }
    }

    bool EnsureActiveDocumentPath(std::shared_ptr<Document>& doc) {
        if (!doc) return false;
        if (!doc->FilePath.empty()) return true;

        std::string path = PromptLine("Save file path first:", "Save Required");
        if (path.empty()) {
            SetStatus("Build/Run cancelled: file is unsaved.");
            return false;
        }

        std::string normalizedPath;
        if (!NormalizePathString(path, normalizedPath)) {
            SetStatus("Build/Run cancelled: invalid file path.");
            return false;
        }

        doc->FilePath = normalizedPath;
        doc->Title = fs::path(normalizedPath).filename().string();
        doc->SaveFile();
        doc->Lang = m_langRegistry.DetectLanguage(doc->FilePath);
        return true;
    }

    std::vector<std::string> SplitOutputLines(const std::string& text) {
        std::vector<std::string> lines;
        std::stringstream ss(text);
        std::string line;
        while (std::getline(ss, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
        }
        if (lines.empty()) {
            lines.push_back("(no output)");
        }
        return lines;
    }

    void ShowCommandOutputDialog(const std::string& title, const std::string& command, const std::string& output, bool ok) {
        int dWidth = (std::min)(m_width - 4, 108);
        int dHeight = (std::min)(m_height - 4, 22);
        int startX = (m_width - dWidth) / 2;
        int startY = (m_height - dHeight) / 2;

        WORD dAttr = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
        for (int y = startY; y < startY + dHeight; ++y) {
            for (int x = startX; x < startX + dWidth; ++x) {
                PutChar(x, y, L' ', dAttr);
            }
        }

        // For Build/Run/Test and all Git commands we don't trust the propagated
        // exit code, so show no verdict tag; the tool's output is the source of truth.
        bool neutral = (title == "Build" || title == "Run" || title == "Test" ||
                        title.rfind("Git", 0) == 0);
        std::string heading = neutral ? title : (title + (ok ? " [OK]" : " [FAILED]"));
        DrawString(startX + 2, startY + 1, heading, dAttr | FOREGROUND_RED);
        DrawString(startX + 2, startY + 2, "Command:", dAttr | FOREGROUND_BLUE);

        int lineMaxLen = dWidth - 4;
        std::string cmdLine = command;
        if ((int)cmdLine.size() > lineMaxLen) {
            cmdLine = cmdLine.substr(0, lineMaxLen - 3) + "...";
        }
        DrawString(startX + 2, startY + 3, cmdLine, dAttr);

        DrawString(startX + 2, startY + 5, "Output:", dAttr | FOREGROUND_BLUE);
        auto lines = SplitOutputLines(output);
        int maxRows = dHeight - 9;
        int startLine = (std::max)(0, static_cast<int>(lines.size()) - maxRows);

        for (int i = 0; i < maxRows && (startLine + i) < static_cast<int>(lines.size()); ++i) {
            std::string shown = lines[startLine + i];
            if ((int)shown.size() > lineMaxLen) {
                shown = shown.substr(0, lineMaxLen - 3) + "...";
            }
            DrawString(startX + 2, startY + 6 + i, shown, dAttr);
        }

        DrawString(startX + 2, startY + dHeight - 2, "Press ESC or ENTER to close...", dAttr);
        FlushBuffer();

        INPUT_RECORD ir;
        DWORD read;
        while (true) {
            ReadInputEvent(ir);
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
                if (vk == VK_ESCAPE || vk == VK_RETURN) {
                    break;
                }
            }
        }

        m_needsRender = true;
    }

    // Show a plain "build output" dialog without a success/failure verdict; the
    // compiler's own text (or the object file it prints) tells the user the result.
    void ShowBuildResultDialog(bool ok) {
        (void)ok;
        int dWidth = (std::min)(m_width - 4, 52);
        int dHeight = 9;
        int startX = (m_width - dWidth) / 2;
        int startY = (m_height - dHeight) / 2;

        WORD dAttr = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
        for (int y = startY; y < startY + dHeight; ++y) {
            for (int x = startX; x < startX + dWidth; ++x) {
                PutChar(x, y, L' ', dAttr);
            }
        }

        std::string title = "Build";
        std::string message = "See compiler output above.";

        DrawString(startX + 2, startY + 1, title, dAttr | FOREGROUND_BLUE);
        DrawString(startX + 2, startY + 3, message, dAttr);
        DrawString(startX + 2, startY + dHeight - 2, "Press ESC or ENTER to continue...", dAttr);
        FlushBuffer();

        INPUT_RECORD ir;
        DWORD read;
        while (true) {
            ReadInputEvent(ir);
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
                if (vk == VK_ESCAPE || vk == VK_RETURN) {
                    break;
                }
            }
        }

        m_needsRender = true;
    }

    bool RunMenuCommandProcess(const std::string& title, const std::string& command) {
        std::string output;
        bool ok = SandboxedExecutor::RunProcess(command, output);

        if (title == "Build" || title == "Link") {
            LogCompilerOutput(title, command, output, ok);
        }

        m_lastToolTitle = title;
        m_lastToolCommand = command;
        m_lastToolOutput = output;
        m_hasLastToolOutput = true;
        ParseDiagnostics(output);

        ShowCommandOutputDialog(title, command, output, ok);
        if (title == "Build" || title == "Run" || title == "Test" || title.rfind("Git", 0) == 0) {
            // No success/failure verdict; the tool's own output above is the
            // source of truth. Just confirm the command ran.
            SetStatus(title + " finished (see output).");
        } else if (ok) {
            SetStatus(title + " completed.");
        } else {
            // Surface the first error/diagnostic line in the status bar so the
            // failure cause is visible without reopening the output dialog.
            std::string firstErr;
            std::stringstream ss(output);
            std::string ln;
            while (std::getline(ss, ln)) {
                if (ln.find("error") != std::string::npos || ln.find("not recognized") != std::string::npos ||
                    ln.find("cannot") != std::string::npos || ln.find("failed") != std::string::npos ||
                    ln.find("No such file") != std::string::npos) {
                    firstErr = ln;
                    break;
                }
            }
            if (firstErr.empty() && !output.empty()) {
                // Fall back to the first non-empty output line.
                std::stringstream ss2(output);
                while (std::getline(ss2, ln)) {
                    if (!ln.empty() && ln.find_first_not_of(" \t\r") != std::string::npos) { firstErr = ln; break; }
                }
            }
            if (firstErr.size() > 120) firstErr = firstErr.substr(0, 117) + "...";
            SetStatus(title + " failed: " + (firstErr.empty() ? "(see output)" : firstErr));
        }
        return ok;
    }

    void ShowLastCompilerOutput() {
        if (!m_hasLastToolOutput) {
            SetStatus("No compiler output captured yet.");
            return;
        }

        ShowCommandOutputDialog("Compiler Output: " + m_lastToolTitle, m_lastToolCommand, m_lastToolOutput, true);
        SetStatus("Showing last compiler output.");
    }

    void ParseDiagnostics(const std::string& output) {
        m_diagnostics.clear();
        m_diagnosticIndex = 0;
        std::stringstream lines(output);
        std::string line;
        while (std::getline(lines, line)) {
            if (line.find("error") == std::string::npos && line.find("warning") == std::string::npos) continue;

            std::string filePath;
            int lineNumber = 0;
            size_t openParen = line.find('(');
            if (openParen != std::string::npos && openParen > 0) {
                size_t digit = openParen + 1;
                while (digit < line.size() && std::isdigit(static_cast<unsigned char>(line[digit]))) ++digit;
                if (digit > openParen + 1 && digit < line.size() && line[digit] == ')') {
                    try { lineNumber = std::stoi(line.substr(openParen + 1, digit - openParen - 1)); } catch (...) { lineNumber = 0; }
                    filePath = line.substr(0, openParen);
                }
            }

            if (lineNumber == 0) {
                size_t firstColon = line.find(':');
                size_t secondColon = firstColon == std::string::npos ? std::string::npos : line.find(':', firstColon + 1);
                if (firstColon != std::string::npos && secondColon != std::string::npos && firstColon > 0) {
                    bool numeric = firstColon + 1 < secondColon;
                    for (size_t i = firstColon + 1; i < secondColon; ++i) {
                        if (!std::isdigit(static_cast<unsigned char>(line[i]))) numeric = false;
                    }
                    if (numeric) {
                        try { lineNumber = std::stoi(line.substr(firstColon + 1, secondColon - firstColon - 1)); } catch (...) { lineNumber = 0; }
                        filePath = line.substr(0, firstColon);
                    }
                }
            }

            if (filePath.empty() || lineNumber <= 0) continue;
            size_t marker = line.find("error:");
            if (marker == std::string::npos) marker = line.find("warning:");
            m_diagnostics.push_back({filePath, lineNumber, marker == std::string::npos ? line : line.substr(marker)});
        }
    }

    void GoToNextDiagnostic() {
        if (m_diagnostics.empty()) {
            SetStatus("No compiler diagnostics available.");
            return;
        }
        const Diagnostic& diagnostic = m_diagnostics[m_diagnosticIndex % m_diagnostics.size()];
        for (int i = 0; i < static_cast<int>(m_documents.size()); ++i) {
            if (m_documents[i] && (m_documents[i]->FilePath == diagnostic.FilePath ||
                fs::path(m_documents[i]->FilePath).filename() == fs::path(diagnostic.FilePath).filename())) {
                m_activeDocIndex = i;
                auto doc = m_documents[i];
                doc->CursorY = (std::max)(0, (std::min)(diagnostic.Line - 1, static_cast<int>(doc->Lines.size()) - 1));
                doc->CursorX = 0;
                EnsureCursorVisible(doc, i);
                SetStatus("Diagnostic " + std::to_string(m_diagnosticIndex + 1) + "/" +
                          std::to_string(m_diagnostics.size()) + ": " + diagnostic.Message);
                m_diagnosticIndex = (m_diagnosticIndex + 1) % m_diagnostics.size();
                return;
            }
        }
        SetStatus("Diagnostic file is not open: " + diagnostic.FilePath);
        m_diagnosticIndex = (m_diagnosticIndex + 1) % m_diagnostics.size();
    }

    void AddCompilerPathOption() {
        std::string folder;
        if (!ShowPathNavigatorDialog("Add Compiler Path", true, true, folder, true)) {
            SetStatus("Add compiler path cancelled.");
            return;
        }

        fs::path p = fs::path(folder);
        try {
            p = fs::absolute(p);
        } catch (const std::exception& ex) {
            SetStatus(std::string("Invalid compiler path: ") + ex.what());
            return;
        } catch (...) {
            SetStatus("Invalid compiler path.");
            return;
        }

        if (!fs::exists(p) || !fs::is_directory(p)) {
            SetStatus("Compiler path is not a folder.");
            return;
        }

        std::string folderStr = p.string();
        if (!IsSafePathSegment(folderStr)) {
            SetStatus("Compiler path contains invalid characters.");
            return;
        }

        if (!IsLikelyCompilerToolDirectory(p)) {
            SetStatus("Folder does not contain known compiler tools.");
            return;
        }

        std::string currentPath = ReadEnvironmentVariableString("PATH");

        auto normalize = [](std::string s) {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            while (!s.empty() && (s.back() == '\\' || s.back() == '/')) s.pop_back();
            return s;
        };

        std::string folderNorm = normalize(folderStr);
        bool alreadyInPath = false;
        std::stringstream ss(currentPath);
        std::string segment;
        while (std::getline(ss, segment, ';')) {
            if (normalize(segment) == folderNorm) {
                alreadyInPath = true;
                break;
            }
        }

        if (alreadyInPath) {
            SetStatus("Compiler path already exists in PATH.");
            return;
        }

        std::string updatedPath = currentPath.empty() ? folderStr : (currentPath + ";" + folderStr);
        if (!SetEnvironmentVariableA("PATH", updatedPath.c_str())) {
            SetStatus("Failed to update PATH.");
            return;
        }

        SetStatus("Compiler path added for this session.");
    }

    std::string FindToolInPath(const std::string& exeName) const {
        std::string pathValue = ReadEnvironmentVariableString("PATH");
        if (pathValue.empty()) return "";

        std::stringstream ss(pathValue);
        std::string item;
        while (std::getline(ss, item, ';')) {
            fs::path p = fs::path(item) / exeName;
            if (fs::exists(p)) {
                return p.string();
            }
        }
        return "";
    }

    std::string DetectDebuggerForLanguage(SupportedLanguage lang) const {
        const auto& cfg = m_langRegistry.GetConfig(lang);
        if (cfg.DebuggerExe.empty()) return "";
        return FindToolInPath(cfg.DebuggerExe);
    }

    std::string DetectLinkerForLanguage(SupportedLanguage lang) const {
        switch (lang) {
        case SupportedLanguage::Cpp:
        case SupportedLanguage::C: {
            std::string link = FindToolInPath("link.exe");
            if (!link.empty()) return link;

            auto tools = CompilerDetector::DetectAll();
            for (const auto& tool : tools) {
                if (tool.Name.find("cl.exe") != std::string::npos && tool.Found) {
                    fs::path clPath(tool.Path);
                    fs::path linkCandidate = clPath.parent_path() / "link.exe";
                    if (fs::exists(linkCandidate)) {
                        return linkCandidate.string();
                    }
                }
            }

            link = FindToolInPath("g++.exe");
            if (!link.empty()) return link;
            return FindToolInPath("gcc.exe");
        }
        case SupportedLanguage::ObjC:
            return FindToolInPath("clang.exe");
        case SupportedLanguage::Swift:
            return FindToolInPath("swiftc.exe");
        case SupportedLanguage::Fortran:
            return FindToolInPath("gfortran.exe");
        case SupportedLanguage::Cobol:
            return FindToolInPath("cobc.exe");
        case SupportedLanguage::Pascal:
            return FindToolInPath("fpc.exe");
        case SupportedLanguage::CSharp:
            return FindToolInPath("csc.exe");
        default:
            return "";
        }
    }

    void ShowDebuggerDetectDialog() {
        std::vector<std::pair<std::string, std::string>> probes = {
            {"C/C++ Debugger (cdb.exe)", "cdb.exe"},
            {"GNU Debugger (gdb.exe)", "gdb.exe"},
            {"LLDB (lldb.exe)", "lldb.exe"},
            {"Java Debugger (jdb.exe)", "jdb.exe"},
            {".NET Debugger (vsdbg.exe)", "vsdbg.exe"},
            {"Node Debugger (node.exe --inspect)", "node.exe"}
        };

        std::vector<CompilerDetector::DetectedTool> detected;
        detected.reserve(probes.size());
        for (const auto& probe : probes) {
            std::string path = FindToolInPath(probe.second);
            detected.push_back({probe.first, path, !path.empty()});
        }

        int contentWidth = 0;
        for (const auto& tool : detected) {
            std::string status = tool.Found ? "[FOUND] " + tool.Path : "[NOT FOUND]";
            std::string line = tool.Name + " : " + status;
            contentWidth = (std::max)(contentWidth, static_cast<int>(line.size()));
        }

        int dWidth = (std::max)(78, contentWidth + 6);
        dWidth = (std::min)(dWidth, m_width - 4);
        int dHeight = (std::max)(12, static_cast<int>(detected.size()) + 7);
        dHeight = (std::min)(dHeight, m_height - 4);
        int startX = (m_width - dWidth) / 2;
        int startY = (m_height - dHeight) / 2;
        WORD dAttr = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;

        for (int y = startY; y < startY + dHeight; ++y) {
            for (int x = startX; x < startX + dWidth; ++x) {
                PutChar(x, y, L' ', dAttr);
            }
        }

        DrawString(startX + 2, startY + 1, "CrossForge Auto-Detected Debuggers", dAttr | FOREGROUND_RED);
        DrawString(startX + 2, startY + 2, "=========================================================", dAttr);

        int lineMaxLen = dWidth - 4;
        int maxRows = (std::min)(static_cast<int>(detected.size()), dHeight - 6);
        for (int i = 0; i < maxRows; ++i) {
            std::string status = detected[i].Found ? "[FOUND] " + detected[i].Path : "[NOT FOUND]";
            std::string line = detected[i].Name + " : " + status;
            if ((int)line.size() > lineMaxLen) {
                line = line.substr(0, (std::max)(0, lineMaxLen - 3)) + "...";
            }
            DrawString(startX + 2, startY + 4 + i, line, dAttr);
        }

        DrawString(startX + 2, startY + dHeight - 2, "Press ESC or ENTER to close...", dAttr);
        FlushBuffer();

        INPUT_RECORD ir;
        DWORD read;
        while (true) {
            ReadInputEvent(ir);
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
                if (vk == VK_ESCAPE || vk == VK_RETURN) break;
            }
        }

        m_needsRender = true;
    }

    void RunDebugForActiveTarget() {
        if (m_documents.empty()) return;
        auto doc = m_documents[m_activeDocIndex];
        if (!EnsureActiveDocumentPath(doc)) return;

        doc->SaveFile();
        doc->Lang = m_langRegistry.DetectLanguage(doc->FilePath);
        const auto& cfg = m_langRegistry.GetConfig(doc->Lang);

        std::string debuggerPath = DetectDebuggerForLanguage(doc->Lang);
        if (debuggerPath.empty()) {
            SetStatus("Debugger not found for " + cfg.Name + ".");
            return;
        }

        fs::path sourcePath = fs::absolute(fs::path(doc->FilePath));
        fs::path workDir = sourcePath.parent_path();
        fs::path exeBase = workDir / sourcePath.stem();

        std::string debugProbeCmd;
        if (cfg.DebuggerExe == "gdb.exe" || cfg.DebuggerExe == "lldb.exe" || cfg.DebuggerExe == "jdb.exe" || cfg.DebuggerExe == "vsdbg.exe") {
            debugProbeCmd = "\"" + debuggerPath + "\" --version";
        } else if (cfg.DebuggerExe == "cdb.exe") {
            debugProbeCmd = "\"" + debuggerPath + "\" -?";
        } else if (cfg.DebuggerExe == "node.exe") {
            debugProbeCmd = "\"" + debuggerPath + "\" --version";
        } else {
            debugProbeCmd = "\"" + debuggerPath + "\" --version";
        }

        std::string command = "cmd.exe /c cd /d \"" + workDir.string() + "\" && " + debugProbeCmd;
        std::string output;
        bool ok = SandboxedExecutor::RunProcess(command, output);
        m_lastDebugTitle = "Debug";
        m_lastDebugCommand = command;
        m_lastDebugOutput = output + "\n\nDebugger template for active language:\n" + cfg.DebugCmdTemplate;
        m_hasLastDebugOutput = true;

        ShowCommandOutputDialog("Debug Probe", command, m_lastDebugOutput, ok);
        SetStatus(ok ? "Debugger detected and ready." : "Debugger probe failed.");
    }

    void ShowLastDebugOutput() {
        if (!m_hasLastDebugOutput) {
            SetStatus("No debug output captured yet.");
            return;
        }
        ShowCommandOutputDialog("Debug Output: " + m_lastDebugTitle, m_lastDebugCommand, m_lastDebugOutput, true);
        SetStatus("Showing last debug output.");
    }

    void RunLinkerForActiveTarget() {
        if (m_documents.empty()) return;
        auto doc = m_documents[m_activeDocIndex];
        if (!EnsureActiveDocumentPath(doc)) return;

        doc->SaveFile();
        doc->Lang = m_langRegistry.DetectLanguage(doc->FilePath);
        const auto& cfg = m_langRegistry.GetConfig(doc->Lang);

        std::string linkerPath = DetectLinkerForLanguage(doc->Lang);
        if (linkerPath.empty()) {
            SetStatus("No linker detected for " + cfg.Name + ".");
            return;
        }

        fs::path sourcePath = fs::absolute(fs::path(doc->FilePath));
        fs::path workDir = sourcePath.parent_path();
        fs::path exeBase = workDir / sourcePath.stem();
        fs::path objFile = exeBase;
        objFile += ".obj";

        std::string linkCmd;
        switch (doc->Lang) {
        case SupportedLanguage::Cpp:
        case SupportedLanguage::C:
            if (linkerPath.find("link.exe") != std::string::npos && fs::exists(objFile)) {
                linkCmd = "\"" + linkerPath + "\" /NOLOGO \"" + objFile.string() + "\" /OUT:\"" + exeBase.string() + ".exe\"";
            } else {
                linkCmd = cfg.CompileCmdTemplate;
                ReplaceAllTokens(linkCmd, "%f", sourcePath.string());
                ReplaceAllTokens(linkCmd, "%e", exeBase.string());
            }
            break;
        case SupportedLanguage::Fortran:
        case SupportedLanguage::Cobol:
        case SupportedLanguage::Pascal:
        case SupportedLanguage::ObjC:
        case SupportedLanguage::Swift:
        case SupportedLanguage::CSharp:
        case SupportedLanguage::Rust:
        case SupportedLanguage::Go:
            linkCmd = cfg.CompileCmdTemplate;
            ReplaceAllTokens(linkCmd, "%f", sourcePath.string());
            ReplaceAllTokens(linkCmd, "%e", exeBase.string());
            break;
        default:
            SetStatus("Linker flow not applicable to " + cfg.Name + ".");
            return;
        }

        if (linkCmd.empty()) {
            SetStatus("No linker command available for " + cfg.Name + ".");
            return;
        }

        std::string command = "cmd.exe /c cd /d \"" + workDir.string() + "\" && " + linkCmd;
        RunMenuCommandProcess("Link", command);
    }

    void BuildActiveTarget() {
        if (m_documents.empty()) return;
        auto doc = m_documents[m_activeDocIndex];
        if (!EnsureActiveDocumentPath(doc)) return;

        doc->SaveFile();
        doc->Lang = m_langRegistry.DetectLanguage(doc->FilePath);
        const auto& cfg = m_langRegistry.GetConfig(doc->Lang);
        if (cfg.CompileCmdTemplate.empty()) {
            SetStatus("No build command for " + cfg.Name + ".");
            return;
        }

        fs::path sourcePath = fs::absolute(fs::path(doc->FilePath));
        fs::path workDir = sourcePath.parent_path();
        fs::path exeBase = workDir / sourcePath.stem();

        if (!fs::exists(workDir) || !fs::is_directory(workDir)) {
            SetStatus("Build failed: source directory does not exist.");
            return;
        }

        std::string buildCmd = cfg.CompileCmdTemplate;
        ReplaceAllTokens(buildCmd, "%f", sourcePath.string());
        ReplaceAllTokens(buildCmd, "%e", exeBase.string());

        // MSVC's cl.exe needs the developer environment (INCLUDE/LIB/PATH) that a
        // Developer Prompt sets up. Without it, cl fails with C1034 "no include
        // path set". Wrap C/C++ builds in VsDevCmd/vcvars so the IDE build matches
        // a working command-line compile. Only applied when INCLUDE isn't already
        // configured (e.g. the IDE itself was launched from a Developer Prompt).
        std::string prefix;
        bool isMsvcCpp = (doc->Lang == SupportedLanguage::Cpp || doc->Lang == SupportedLanguage::C) &&
                         buildCmd.find("cl.exe") != std::string::npos;
        if (isMsvcCpp && GetEnvironmentVariableA("INCLUDE", nullptr, 0) == 0) {
            static const std::string devCmd = CompilerDetector::FindVsDevCmdScript();
            if (!devCmd.empty()) {
                prefix = "call \"" + devCmd + "\" -no_logo && ";
            }
        }

        // Run the build and just show the compiler's output. We deliberately do
        // NOT try to guess success/failure: the cmd/call chain makes the process
        // exit code unreliable, and the compiler's own text (or the object it
        // prints) is the source of truth the user asked to see.
        std::string command = "cmd.exe /c cd /d \"" + workDir.string() + "\" && " + prefix +
                              buildCmd;
        RunMenuCommandProcess("Build", command);
    }

    void RunActiveTarget() {
        if (m_documents.empty()) return;
        auto doc = m_documents[m_activeDocIndex];
        if (!EnsureActiveDocumentPath(doc)) return;

        doc->SaveFile();
        doc->Lang = m_langRegistry.DetectLanguage(doc->FilePath);

        fs::path sourcePath = fs::absolute(fs::path(doc->FilePath));
        fs::path workDir = sourcePath.parent_path();
        fs::path exeBase = workDir / sourcePath.stem();

        std::string runCmd;
        switch (doc->Lang) {
        case SupportedLanguage::JavaScript:
        case SupportedLanguage::NodeJS:
            runCmd = "node \"" + sourcePath.string() + "\"";
            break;
        case SupportedLanguage::TypeScript: {
            fs::path jsFile = exeBase;
            jsFile += ".js";
            if (!fs::exists(jsFile)) {
                SetStatus("Run failed: build TypeScript first.");
                return;
            }
            runCmd = "node \"" + jsFile.string() + "\"";
            break;
        }
        case SupportedLanguage::Java:
            runCmd = "java \"" + sourcePath.stem().string() + "\"";
            break;
        default: {
            fs::path exeFile = exeBase;
            exeFile += ".exe";
            if (!fs::exists(exeFile)) {
                SetStatus("Run failed: build target first.");
                return;
            }
            runCmd = "\"" + exeFile.string() + "\"";
            break;
        }
        }

        std::string command = "cmd.exe /c cd /d \"" + workDir.string() + "\" && " + runCmd;
        RunMenuCommandProcess("Run", command);
    }

    void TestActiveTarget() {
        if (m_documents.empty()) return;
        auto doc = m_documents[m_activeDocIndex];
        if (!EnsureActiveDocumentPath(doc)) return;

        fs::path sourcePath = fs::absolute(fs::path(doc->FilePath));
        fs::path workDir = sourcePath.parent_path();
        fs::path exeBase = workDir / sourcePath.stem();

        std::string testCmd;
        if (fs::exists(workDir / "package.json")) {
            testCmd = "npm test";
        } else if (fs::exists(workDir / "CTestTestfile.cmake") || fs::exists(workDir / "CMakeLists.txt")) {
            testCmd = "ctest --output-on-failure";
        } else {
            fs::path exeFile = exeBase;
            exeFile += ".exe";
            if (!fs::exists(exeFile)) {
                SetStatus("Test failed: no test command or executable found.");
                return;
            }
            testCmd = "\"" + exeFile.string() + "\" --test";
        }

        std::string command = "cmd.exe /c cd /d \"" + workDir.string() + "\" && " + testCmd;
        RunMenuCommandProcess("Test", command);
    }

    // ========================================================================
    // GIT INTEGRATION
    // ========================================================================

    // Resolve the working directory for Git commands: the active document's folder
    // if it has one, otherwise the current process directory.
    fs::path GitWorkDir() const {
        if (!m_documents.empty()) {
            const auto& doc = m_documents[m_activeDocIndex];
            if (doc && !doc->FilePath.empty()) {
                std::error_code ec;
                fs::path dir = fs::path(doc->FilePath).parent_path();
                if (!dir.empty() && fs::exists(dir, ec)) return dir;
            }
        }
        std::error_code ec;
        fs::path cwd = fs::current_path(ec);
        return ec ? fs::path(".") : cwd;
    }

    // Run a git subcommand in the working directory and show its output.
    // Preflight: verify git is available so the user gets a clear message instead
    // of a failed 'git is not recognized' command dialog.
    void RunGitCommand(const std::string& title, const std::string& args) {
        if (FindToolInPath("git.exe").empty() && FindToolInPath("git.cmd").empty()
            && FindToolInPath("git.bat").empty()) {
            ShowCommandOutputDialog(title, "git " + args,
                "Git was not found on PATH.\n\n"
                "Install Git for Windows (https://git-scm.com/download/win) or add "
                "the folder containing git.exe to PATH, then restart CrossForge.",
                false);
            SetStatus("Git not found on PATH.");
            return;
        }
        fs::path workDir = GitWorkDir();
        std::string command = "cmd.exe /c cd /d \"" + workDir.string() + "\" && git " + args;
        RunMenuCommandProcess(title, command);
    }

    void GitStatus() { RunGitCommand("Git Status", "status"); }
    void GitPull()   { RunGitCommand("Git Pull", "pull"); }
    void GitPush()   { RunGitCommand("Git Push", "push"); }
    void GitLog()    { RunGitCommand("Git Log", "log --oneline --graph --decorate -n 40"); }
    void GitDiff()   { RunGitCommand("Git Diff", "diff"); }

    void GitAddAll() {
        RunGitCommand("Git Stage All", "add -A");
        SetStatus("Staged all changes.");
    }

    void GitCommit() {
        std::string message = PromptLine("Commit message:", "Git Commit");
        if (message.empty()) {
            SetStatus("Commit cancelled: empty message.");
            return;
        }
        // Keep the message on one line (git treats it as the subject).
        size_t nl = message.find_first_of("\r\n");
        if (nl != std::string::npos) message = message.substr(0, nl);
        if (message.empty()) {
            SetStatus("Commit cancelled: empty message.");
            return;
        }
        // Avoid shell quoting entirely: write the message to a temp file and use
        // `git commit -F <file>`, so no metacharacter in the message can inject.
        std::error_code ec;
        fs::path tmpDir = fs::temp_directory_path(ec);
        if (ec) { SetStatus("Commit failed: no temp dir."); return; }
        fs::path msgFile = tmpDir / ("crossforge_commit_" + std::to_string(GetCurrentProcessId()) + ".txt");
        {
            std::ofstream mf(msgFile, std::ios::binary | std::ios::trunc);
            if (!mf.is_open()) { SetStatus("Commit failed: cannot write message file."); return; }
            mf << message;
            mf.close();
            if (mf.fail()) { fs::remove(msgFile, ec); SetStatus("Commit failed: cannot write message file."); return; }
        }
        RunGitCommand("Git Commit", "commit -F \"" + msgFile.string() + "\"");
        fs::remove(msgFile, ec); // best-effort cleanup
    }

    bool ConfirmDocumentClose(int index) {
        if (index < 0 || index >= static_cast<int>(m_documents.size())) return true;
        auto doc = m_documents[index];
        if (!doc || !doc->IsModified) return true;

        int previousIndex = m_activeDocIndex;
        m_activeDocIndex = index;
        std::string answer = PromptLine("Save changes to " + doc->Title + "? (y/n/c)", "Unsaved Changes");
        if (!answer.empty()) {
            char choice = static_cast<char>(std::tolower(static_cast<unsigned char>(answer[0])));
            if (choice == 'y') {
                SaveActiveDocument();
                bool saved = !doc->IsModified;
                m_activeDocIndex = previousIndex;
                return saved;
            }
            if (choice == 'n') {
                m_activeDocIndex = previousIndex;
                return true;
            }
        }
        m_activeDocIndex = previousIndex;
        SetStatus("Close cancelled.");
        return false;
    }

    void RequestExit() {
        for (int i = 0; i < static_cast<int>(m_documents.size()); ++i) {
            if (!ConfirmDocumentClose(i)) return;
        }
        m_running = false;
    }

    void CloseActiveDocument() {
        if (m_documents.empty()) return;
        if (!ConfirmDocumentClose(m_activeDocIndex)) return;
        std::string title = m_documents[m_activeDocIndex]->Title;
        m_documents.erase(m_documents.begin() + m_activeDocIndex);
        if (m_documents.empty()) {
            auto doc = std::make_shared<Document>(1, "");
            doc->Lines.resetSingle("");
            doc->Title = "NO NAME";
            doc->Lang = SupportedLanguage::Text;
            m_documents.push_back(doc);
            m_activeDocIndex = 0;
        } else if (m_activeDocIndex >= static_cast<int>(m_documents.size())) {
            m_activeDocIndex = static_cast<int>(m_documents.size()) - 1;
        }
        SetStatus("Closed: " + title);
        m_needsRender = true;
    }

    void CloseDocumentAtIndex(int index) {
        if (m_documents.empty()) return;
        if (index < 0 || index >= static_cast<int>(m_documents.size())) return;

        m_activeDocIndex = index;
        CloseActiveDocument();
    }

    void InsertLineBelow() {
        if (m_documents.empty()) return;
        auto doc = m_documents[m_activeDocIndex];
        EnsureDocumentInvariants(doc);
        doc->RecordEdit();
        doc->InvalidateLineStatesFrom(static_cast<size_t>(doc->CursorY));
        doc->Lines.insert(doc->CursorY + 1, "");
        doc->CursorY++;
        doc->CursorX = 0;
        doc->IsModified = true;
        SetStatus("Inserted line.");
        m_selectAllActive = false;
    }

    void DeleteCurrentLine() {
        if (m_documents.empty()) return;
        auto doc = m_documents[m_activeDocIndex];
        EnsureDocumentInvariants(doc);
        doc->RecordEdit();
        doc->InvalidateLineStatesFrom(static_cast<size_t>(doc->CursorY));
        doc->Lines.erase(doc->CursorY);
        if (doc->Lines.empty()) doc->Lines.push_back("");
        if (doc->CursorY >= static_cast<int>(doc->Lines.size())) {
            doc->CursorY = static_cast<int>(doc->Lines.size()) - 1;
        }
        doc->CursorX = (std::min)(doc->CursorX, static_cast<int>(doc->Lines[doc->CursorY].size()));
        doc->IsModified = true;
        SetStatus("Deleted line.");
        m_selectAllActive = false;
    }

    void DuplicateCurrentLine() {
        if (m_documents.empty()) return;
        auto doc = m_documents[m_activeDocIndex];
        EnsureDocumentInvariants(doc);
        doc->RecordEdit();
        doc->InvalidateLineStatesFrom(static_cast<size_t>(doc->CursorY));
        doc->Lines.insert(doc->CursorY + 1, doc->Lines[doc->CursorY]);
        doc->CursorY++;
        doc->IsModified = true;
        SetStatus("Duplicated line.");
        m_selectAllActive = false;
    }

    void CopySelectionOrLine() {
        if (m_documents.empty()) return;
        auto doc = m_documents[m_activeDocIndex];
        EnsureDocumentInvariants(doc);

        if (m_selectAllActive) {
            m_clipboardText.clear();
            for (size_t i = 0; i < doc->Lines.size(); ++i) {
                m_clipboardText += doc->Lines[i];
                if (i + 1 < doc->Lines.size()) m_clipboardText += '\n';
            }
            SetStatus("Copied all.");
            SetSystemClipboardText(m_clipboardText);
            return;
        }

        m_clipboardText = doc->Lines[doc->CursorY];
        SetSystemClipboardText(m_clipboardText);
        SetStatus("Copied line.");
    }

    void CutSelectionOrLine() {
        if (m_documents.empty()) return;
        auto doc = m_documents[m_activeDocIndex];
        EnsureDocumentInvariants(doc);

        if (m_selectAllActive) {
            doc->RecordEdit();
            m_clipboardText.clear();
            for (size_t i = 0; i < doc->Lines.size(); ++i) {
                m_clipboardText += doc->Lines[i];
                if (i + 1 < doc->Lines.size()) m_clipboardText += '\n';
            }
            doc->Lines.assignSingle("");
            doc->CursorX = 0;
            doc->CursorY = 0;
            doc->InvalidateLineStatesFrom(0);
            doc->IsModified = true;
            m_selectAllActive = false;
            SetSystemClipboardText(m_clipboardText);
            SetStatus("Cut all.");
            return;
        }

        doc->RecordEdit();
        m_clipboardText = doc->Lines[doc->CursorY];
        SetSystemClipboardText(m_clipboardText);
        doc->InvalidateLineStatesFrom(static_cast<size_t>(doc->CursorY));
        doc->Lines.erase(doc->CursorY);
        if (doc->Lines.empty()) doc->Lines.push_back("");
        if (doc->CursorY >= static_cast<int>(doc->Lines.size())) doc->CursorY = static_cast<int>(doc->Lines.size()) - 1;
        doc->CursorX = (std::min)(doc->CursorX, static_cast<int>(doc->Lines[doc->CursorY].size()));
        doc->IsModified = true;
        m_selectAllActive = false;
        SetStatus("Cut line.");
    }

    void PasteClipboardAtCursor() {
        if (m_documents.empty()) return;
        std::string systemClipboard;
        if (GetSystemClipboardText(systemClipboard)) {
            m_clipboardText = systemClipboard;
        }
        if (m_clipboardText.empty()) {
            SetStatus("Clipboard empty.");
            return;
        }

        auto doc = m_documents[m_activeDocIndex];
        EnsureDocumentInvariants(doc);
        doc->RecordEdit();
        doc->InvalidateLineStatesFrom(static_cast<size_t>(doc->CursorY));
        std::string before = doc->Lines[doc->CursorY].substr(0, doc->CursorX);
        std::string after = doc->Lines[doc->CursorY].substr(doc->CursorX);

        std::vector<std::string> parts;
        std::stringstream ss(m_clipboardText);
        std::string part;
        while (std::getline(ss, part)) {
            if (!part.empty() && part.back() == '\r') part.pop_back();
            parts.push_back(part);
        }
        if (parts.empty()) parts.push_back("");

        if (parts.size() == 1) {
            doc->Lines.set(doc->CursorY, before + parts[0] + after);
            doc->CursorX += static_cast<int>(parts[0].size());
        } else {
            doc->Lines.set(doc->CursorY, before + parts[0]);
            int insertAt = doc->CursorY + 1;
            for (size_t i = 1; i + 1 < parts.size(); ++i) {
                doc->Lines.insert(insertAt, parts[i]);
                ++insertAt;
            }
            doc->Lines.insert(insertAt, parts.back() + after);
            doc->CursorY = insertAt;
            doc->CursorX = static_cast<int>(parts.back().size());
        }

        doc->IsModified = true;
        m_selectAllActive = false;
        SetStatus("Pasted.");
    }

    void SelectAllBuffer() {
        if (m_documents.empty()) return;
        m_selectAllActive = true;
        SetStatus("Selected all.");
    }

    void FindTextPrompt() {
        if (m_documents.empty()) return;
        std::string term = PromptLine("Find text:", "Find");
        if (term.empty()) {
            SetStatus("Find cancelled.");
            return;
        }
        m_lastSearchTerm = term;
        FindNext(false);
    }

    void FindNext(bool reverse) {
        if (m_documents.empty()) return;
        if (m_lastSearchTerm.empty()) {
            FindTextPrompt();
            return;
        }

        auto doc = m_documents[m_activeDocIndex];
        const int lineCount = static_cast<int>(doc->Lines.size());
        if (lineCount == 0) return;
        int startLine = doc->CursorY;
        int startColumn = doc->CursorX;

        for (int step = 0; step < lineCount; ++step) {
            int line = reverse
                ? (startLine - step + lineCount) % lineCount
                : (startLine + step) % lineCount;
            size_t position = std::string::npos;
            if (reverse) {
                size_t end = (line == startLine) ? static_cast<size_t>(startColumn) : doc->Lines[line].size();
                size_t candidate = doc->Lines[line].rfind(m_lastSearchTerm, end);
                if (candidate != std::string::npos && line == startLine && candidate == static_cast<size_t>(startColumn)) {
                    candidate = (candidate == 0) ? std::string::npos : doc->Lines[line].rfind(m_lastSearchTerm, candidate - 1);
                }
                position = candidate;
            } else {
                size_t begin = (line == startLine) ? static_cast<size_t>(startColumn) : 0;
                position = doc->Lines[line].find(m_lastSearchTerm, begin);
                if (position != std::string::npos && line == startLine && position == begin && begin < doc->Lines[line].size()) {
                    position = doc->Lines[line].find(m_lastSearchTerm, begin + 1);
                }
            }

            if (position != std::string::npos) {
                doc->CursorY = line;
                doc->CursorX = static_cast<int>(position);
                EnsureCursorVisible(doc, m_activeDocIndex);
                SetStatus((reverse ? "Previous: " : "Found: ") + m_lastSearchTerm);
                return;
            }
        }
        SetStatus("Not found: " + m_lastSearchTerm);
    }

    void ReplaceTextPrompt() {
        if (m_documents.empty()) return;
        auto doc = m_documents[m_activeDocIndex];
        std::string findText = PromptLine("Find text:", "Replace");
        if (findText.empty()) {
            SetStatus("Replace cancelled.");
            return;
        }
        std::string replaceText = PromptLine("Replace with:", "Replace");
        int count = 0;
        if (!doc->Lines.empty()) doc->RecordEdit();
        doc->Lines.replaceEach([&](const std::string& line) {
            std::string out = line;
            size_t pos = 0;
            while ((pos = out.find(findText, pos)) != std::string::npos) {
                out.replace(pos, findText.size(), replaceText);
                pos += replaceText.size();
                count++;
            }
            return out;
        });
        doc->InvalidateLineStatesFrom(0);
        if (count > 0) doc->IsModified = true;
        SetStatus("Replaced " + std::to_string(count) + " occurrence(s).");
    }

    void ShowDiagnosticsDialog() {
        DWORD processId = GetCurrentProcessId();
        PROCESS_MEMORY_COUNTERS_EX memoryCounters{};
        bool memoryAvailable = K32GetProcessMemoryInfo(
            GetCurrentProcess(),
            reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&memoryCounters),
            sizeof(memoryCounters)) != FALSE;

        DWORD threadCount = 0;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot != INVALID_HANDLE_VALUE) {
            THREADENTRY32 entry{};
            entry.dwSize = sizeof(entry);
            if (Thread32First(snapshot, &entry)) {
                do {
                    if (entry.th32OwnerProcessID == processId) ++threadCount;
                } while (Thread32Next(snapshot, &entry));
            }
            CloseHandle(snapshot);
        }

        bool validEditorState = m_running && !m_documents.empty() &&
                                m_activeDocIndex >= 0 &&
                                m_activeDocIndex < static_cast<int>(m_documents.size());
        bool stable = validEditorState && memoryAvailable && threadCount > 0;
        std::string memoryLine = memoryAvailable
            ? std::to_string(static_cast<unsigned long long>(memoryCounters.WorkingSetSize / (1024 * 1024))) + " MB working set"
            : "Unavailable";

        int dWidth = (std::min)(m_width, 72);
        int dHeight = (std::min)(m_height, 12);
        int startX = (m_width - dWidth) / 2;
        int startY = (m_height - dHeight) / 2;
        WORD dAttr = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;
        bool prevCursorVisible = SetCursorVisible(false);
        std::vector<std::string> lines = {
            "IDE DIAGNOSTICS",
            "",
            "Process ID       : " + std::to_string(processId),
            "Memory used      : " + memoryLine,
            "Thread count     : " + std::to_string(threadCount),
            std::string("Editor stability : ") + (stable ? "STABLE" : "NEEDS ATTENTION"),
            "",
            stable ? "Runtime checks passed." : "One or more runtime checks failed.",
            "",
            "Press ESC or ENTER to close."
        };

        for (int y = startY; y < startY + dHeight; ++y) {
            for (int x = startX; x < startX + dWidth; ++x) {
                PutChar(x, y, L' ', dAttr);
            }
        }
        DrawString(startX + 2, startY + 1, lines[0], dAttr | FOREGROUND_RED);
        DrawString(startX + 2, startY + 2, "===============================================", dAttr);
        for (int i = 2; i < static_cast<int>(lines.size()); ++i) {
            if (startY + i + 1 >= startY + dHeight - 1) break;
            DrawString(startX + 2, startY + i + 1, lines[i], dAttr);
        }
        FlushBuffer();

        while (true) {
            INPUT_RECORD ir;
            if (!ReadInputEvent(ir)) break;
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown &&
                (ir.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE ||
                 ir.Event.KeyEvent.wVirtualKeyCode == VK_RETURN)) {
                break;
            }
        }

        SetCursorVisible(prevCursorVisible);
        m_needsRender = true;
    }

    std::vector<MenuEntry> GetMenuEntries(TopMenuAction menu) const {
        switch (menu) {
        case TopMenuAction::File:
            return {{"New                Ctrl+N", (int)MenuCommand::FileNew},
                {"Open...            Ctrl+O", (int)MenuCommand::FileOpen},
                {"Recent File         Ctrl+E", (int)MenuCommand::FileRecent},
                {"Save               Ctrl+S", (int)MenuCommand::FileSave},
                {"Save As...    Ctrl+Shift+S", (int)MenuCommand::FileSaveAs},
                {"Close              Ctrl+W", (int)MenuCommand::FileClose},
                {"Exit                Alt+X", (int)MenuCommand::FileExit}};
        case TopMenuAction::Edit:
            return {{"Cut                Ctrl+X", (int)MenuCommand::EditCut},
                    {"Copy         Ctrl+Shift+C", (int)MenuCommand::EditCopy},
                {"Paste              Ctrl+V", (int)MenuCommand::EditPaste},
                {"Undo               Ctrl+Z", (int)MenuCommand::EditUndo},
                {"Redo               Ctrl+Y", (int)MenuCommand::EditRedo},
                {"Select All         Ctrl+A", (int)MenuCommand::EditSelectAll},
                {"Insert Line  Ctrl+Shift+I", (int)MenuCommand::EditInsertLine},
                {"Delete Line Ctrl+Shift+Bksp", (int)MenuCommand::EditDeleteLine},
                {"Duplicate Line Ctrl+Shift+U", (int)MenuCommand::EditDuplicateLine}};
        case TopMenuAction::Search:
            return {{"Find...            Ctrl+F", (int)MenuCommand::SearchFind},
                {"Find Next           Ctrl+G", (int)MenuCommand::SearchFindNext},
                {"Find Previous  Ctrl+Shift+G", (int)MenuCommand::SearchFindPrevious},
                {"Replace...         Ctrl+H", (int)MenuCommand::SearchReplace}};
        case TopMenuAction::Run:
            return {{"Terminal              F8", (int)MenuCommand::RunTerminal},
                {"WSL Terminal", (int)MenuCommand::RunWslTerminal},
                {"SSH Terminal...", (int)MenuCommand::RunSshTerminal},
                {"Compiler Output Ctrl+Shift+O", (int)MenuCommand::ViewCompilerOutput},
                {"Next Diagnostic", (int)MenuCommand::ViewNextDiagnostic}};
        case TopMenuAction::Compile:
            return {{"Build         F9 / Ctrl+B", (int)MenuCommand::CompileBuild},
                {"Run          F10 / Ctrl+R", (int)MenuCommand::CompileRun},
                {"Test              Ctrl+T", (int)MenuCommand::CompileTest},
                {"Build Tools...      Alt+F9", (int)MenuCommand::BuildDetect}};
        case TopMenuAction::Git:
            return {{"Status", (int)MenuCommand::GitStatus},
                {"Stage All (add -A)", (int)MenuCommand::GitAddAll},
                {"Commit...", (int)MenuCommand::GitCommit},
                {"Pull", (int)MenuCommand::GitPull},
                {"Push", (int)MenuCommand::GitPush},
                {"Log (graph)", (int)MenuCommand::GitLog},
                {"Diff", (int)MenuCommand::GitDiff}};
        case TopMenuAction::Debug:
            return {{"Debug Current   Ctrl+Alt+D", (int)MenuCommand::DebugRunActive},
                {"View Debug Output Ctrl+Shift+Y", (int)MenuCommand::DebugViewOutput},
                {"Debugger Tools... Ctrl+Shift+D", (int)MenuCommand::DebugDetect}};
        case TopMenuAction::Options:
            return {{"Next Theme        Ctrl+Alt+T", (int)MenuCommand::OptionsNextTheme},
                {"Add Compiler Path Ctrl+Alt+P", (int)MenuCommand::OptionsAddCompilerPath},
                {"Run Linker Cmd      Ctrl+L", (int)MenuCommand::OptionsRunLinker}};
        case TopMenuAction::Window:
            return {{"Next Window          Ctrl+E", (int)MenuCommand::WindowNext},
                {"New Window      Ctrl+Alt+N", (int)MenuCommand::WindowNew},
                {"Layout Split/Max/Tab", (int)MenuCommand::WindowLayoutToggle}};
        case TopMenuAction::Help:
            return {{"Help Topics           F1", (int)MenuCommand::HelpTopics},
                {"IDE Diagnostics        F4", (int)MenuCommand::HelpDiagnostics},
                {"About           Ctrl+Alt+A", (int)MenuCommand::HelpAbout}};
        case TopMenuAction::None:
        default:
            return {};
        }
    }

    void ExecuteMenuCommand(MenuCommand cmd) {
        switch (cmd) {
        case MenuCommand::FileNew: NewDocument(); SetStatus("New document."); break;
        case MenuCommand::FileOpen: OpenDocumentFromPrompt(); break;
        case MenuCommand::FileSave: SaveActiveDocument(); break;
        case MenuCommand::FileSaveAs: {
            SaveActiveDocumentAs("");
            break;
        }
        case MenuCommand::FileClose: CloseActiveDocument(); break;
        case MenuCommand::FileExit: RequestExit(); break;
        case MenuCommand::FileRecent:
            if (!m_recentFiles.empty()) {
                if (m_recentIndex >= m_recentFiles.size()) m_recentIndex = 0;
                std::string path = m_recentFiles[m_recentIndex++];
                if (fs::exists(path)) {
                    auto doc = std::make_shared<Document>(static_cast<int>(m_documents.size()) + 1, path);
                    doc->Lang = m_langRegistry.DetectLanguage(path);
                    m_documents.push_back(doc);
                    m_activeDocIndex = static_cast<int>(m_documents.size()) - 1;
                    SetStatus("Opened recent file: " + doc->Title);
                } else {
                    SetStatus("Recent file no longer exists.");
                }
            } else {
                SetStatus("No recent files.");
            }
            break;
        case MenuCommand::EditInsertLine: InsertLineBelow(); break;
        case MenuCommand::EditDeleteLine: DeleteCurrentLine(); break;
        case MenuCommand::EditDuplicateLine: DuplicateCurrentLine(); break;
        case MenuCommand::EditUndo: UndoActiveDocument(); break;
        case MenuCommand::EditRedo: RedoActiveDocument(); break;
        case MenuCommand::EditCut: CutSelectionOrLine(); break;
        case MenuCommand::EditCopy: CopySelectionOrLine(); break;
        case MenuCommand::EditPaste: PasteClipboardAtCursor(); break;
        case MenuCommand::EditSelectAll: SelectAllBuffer(); break;
        case MenuCommand::SearchFind: FindTextPrompt(); break;
        case MenuCommand::SearchFindNext: FindNext(false); break;
        case MenuCommand::SearchFindPrevious: FindNext(true); break;
        case MenuCommand::SearchReplace: ReplaceTextPrompt(); break;
        case MenuCommand::RunTerminal: LaunchTerminalIntegration(); break;
        case MenuCommand::RunWslTerminal: LaunchWslTerminal(); break;
        case MenuCommand::RunSshTerminal: LaunchSshTerminal(); break;
        case MenuCommand::ViewCompilerOutput: ShowLastCompilerOutput(); break;
        case MenuCommand::ViewNextDiagnostic: GoToNextDiagnostic(); break;
        case MenuCommand::CompileBuild: BuildActiveTarget(); break;
        case MenuCommand::CompileRun: RunActiveTarget(); break;
        case MenuCommand::CompileTest: TestActiveTarget(); break;
        case MenuCommand::BuildDetect: ShowCompilerDetectDialog(); break;
        case MenuCommand::GitStatus: GitStatus(); break;
        case MenuCommand::GitAddAll: GitAddAll(); break;
        case MenuCommand::GitCommit: GitCommit(); break;
        case MenuCommand::GitPull: GitPull(); break;
        case MenuCommand::GitPush: GitPush(); break;
        case MenuCommand::GitLog: GitLog(); break;
        case MenuCommand::GitDiff: GitDiff(); break;
        case MenuCommand::DebugRunActive: RunDebugForActiveTarget(); break;
        case MenuCommand::DebugViewOutput: ShowLastDebugOutput(); break;
        case MenuCommand::DebugDetect: ShowDebuggerDetectDialog(); break;
        case MenuCommand::OptionsNextTheme: CycleTheme(); SetStatus("Theme changed."); break;
        case MenuCommand::OptionsAddCompilerPath: AddCompilerPathOption(); break;
        case MenuCommand::OptionsRunLinker: RunLinkerForActiveTarget(); break;
        case MenuCommand::WindowNext:
            if (!m_documents.empty()) m_activeDocIndex = (m_activeDocIndex + 1) % m_documents.size();
            SetStatus("Switched window.");
            break;
        case MenuCommand::WindowNew: NewDocument(); SetStatus("New window created."); break;
        case MenuCommand::WindowLayoutToggle: CycleWindowLayout(); break;
        case MenuCommand::HelpTopics: ShowHelpDialog(); break;
        case MenuCommand::HelpDiagnostics: ShowDiagnosticsDialog(); break;
        case MenuCommand::HelpAbout: ShowAboutDialog(); break;
        case MenuCommand::None:
        default:
            break;
        }
        m_needsRender = true;
    }

    void ShowMenuPopup(TopMenuAction menu) {
        auto entries = GetMenuEntries(menu);
        if (entries.empty()) return;
        bool prevCursorVisible = SetCursorVisible(false);

        if (m_needsRender) {
            RenderScreen();
            m_needsRender = false;
        }

        std::vector<CHAR_INFO> popupBaseBuffer = m_screenBuffer;

        int menuStartX = 1;
        int menuEndX = 1;
        if (!TryGetTopMenuRegionX(menu, menuStartX, menuEndX)) return;

        int boxX = menuStartX;
        int boxY = 1;
        int innerWidth = 0;
        for (const auto& e : entries) {
            innerWidth = (std::max)(innerWidth, static_cast<int>(e.Label.size()));
        }
        int boxWidth = innerWidth + 4;
        int boxHeight = static_cast<int>(entries.size()) + 2;
        if (boxX + boxWidth >= m_width) boxX = (std::max)(0, m_width - boxWidth - 1);
        if (boxY + boxHeight >= m_height - 1) boxY = (std::max)(1, m_height - boxHeight - 2);

        int selected = 0;
        bool done = false;
        bool popupDirty = true;
        while (!done) {
            if (popupDirty) {
                if (popupBaseBuffer.size() == m_screenBuffer.size()) {
                    m_screenBuffer = popupBaseBuffer;
                }

                WORD boxAttr = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_BLUE;
                WORD itemAttr = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;

                PutChar(boxX, boxY, '+', boxAttr);
                PutChar(boxX + boxWidth - 1, boxY, '+', boxAttr);
                PutChar(boxX, boxY + boxHeight - 1, '+', boxAttr);
                PutChar(boxX + boxWidth - 1, boxY + boxHeight - 1, '+', boxAttr);
                for (int x = boxX + 1; x < boxX + boxWidth - 1; ++x) {
                    PutChar(x, boxY, '-', boxAttr);
                    PutChar(x, boxY + boxHeight - 1, '-', boxAttr);
                }
                for (int y = boxY + 1; y < boxY + boxHeight - 1; ++y) {
                    PutChar(boxX, y, '|', boxAttr);
                    PutChar(boxX + boxWidth - 1, y, '|', boxAttr);
                    for (int x = boxX + 1; x < boxX + boxWidth - 1; ++x) {
                        PutChar(x, y, ' ', itemAttr);
                    }
                }

                for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
                    std::string label = " " + entries[i].Label;
                    while (static_cast<int>(label.size()) < innerWidth + 1) label += ' ';
                    WORD attr = (i == selected)
                        ? static_cast<WORD>(BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE | FOREGROUND_BLUE)
                        : itemAttr;
                    DrawString(boxX + 1, boxY + 1 + i, label, attr);
                }

                FlushBuffer();
                popupDirty = false;
            }

            if (WaitForSingleObject(m_hStdIn, 40) != WAIT_OBJECT_0) {
                continue;
            }

            INPUT_RECORD ir;
            DWORD read;
            ReadInputEvent(ir);

            if (ir.EventType == WINDOW_BUFFER_SIZE_EVENT) {
                UpdateBufferSize();
                RenderScreen();
                m_needsRender = false;
                popupBaseBuffer = m_screenBuffer;
                popupDirty = true;
                continue;
            }

            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
                if (vk == VK_ESCAPE) {
                    done = true;
                } else if (vk == VK_UP) {
                    selected = (selected - 1 + static_cast<int>(entries.size())) % static_cast<int>(entries.size());
                    popupDirty = true;
                } else if (vk == VK_DOWN) {
                    selected = (selected + 1) % static_cast<int>(entries.size());
                    popupDirty = true;
                } else if (vk == VK_RETURN) {
                    ExecuteMenuCommand(static_cast<MenuCommand>(entries[selected].CommandId));
                    done = true;
                }
            } else if (ir.EventType == MOUSE_EVENT) {
                COORD pos = ir.Event.MouseEvent.dwMousePosition;
                DWORD btn = ir.Event.MouseEvent.dwButtonState;
                DWORD flags = ir.Event.MouseEvent.dwEventFlags;

                if (flags == MOUSE_MOVED) {
                    if (pos.X > boxX && pos.X < boxX + boxWidth - 1 && pos.Y > boxY && pos.Y < boxY + boxHeight - 1) {
                        int idx = pos.Y - (boxY + 1);
                        if (idx >= 0 && idx < static_cast<int>(entries.size()) && idx != selected) {
                            selected = idx;
                            popupDirty = true;
                        }
                    }
                }

                if ((btn & FROM_LEFT_1ST_BUTTON_PRESSED) != 0 && flags == 0) {
                    if (pos.X > boxX && pos.X < boxX + boxWidth - 1 && pos.Y > boxY && pos.Y < boxY + boxHeight - 1) {
                        int idx = pos.Y - (boxY + 1);
                        if (idx >= 0 && idx < static_cast<int>(entries.size())) {
                            selected = idx;
                            ExecuteMenuCommand(static_cast<MenuCommand>(entries[selected].CommandId));
                        }
                    }
                    done = true;
                }
            }
        }

        SetCursorVisible(prevCursorVisible);
        m_needsRender = true;
    }

    int HitTestWindowCloseButton(int x, int y) const {
        if (m_documents.empty()) return -1;

        int numDocs = static_cast<int>(m_documents.size());
        for (int i = 0; i < numDocs; ++i) {
            int topY = 0, botY = 0;
            if (!GetDocWindowFrame(i, topY, botY)) continue; // not visible here
            if (y != topY) continue;

            int closeStartX = m_width - 6;
            int closeEndX = m_width - 4;
            if (x >= closeStartX && x <= closeEndX) {
                return i;
            }
        }

        return -1;
    }

    // In Tabs layout, return the document index whose tab was clicked, else -1.
    int HitTestTabStrip(int x, int y) const {
        if (m_windowLayout != WindowLayout::Tabs || y != 1) return -1;
        int curX = 1;
        for (int i = 0; i < static_cast<int>(m_documents.size()); ++i) {
            const auto& d = m_documents[i];
            std::string label = " " + d->Title + (d->IsModified ? "*" : "") + " ";
            int endX = curX + static_cast<int>(label.size());
            if (x >= curX && x < endX) return i;
            curX = endX + 1;
            if (curX >= m_width - 1) break;
        }
        return -1;
    }

    TopMenuAction HitTestTopMenu(int x) const {
        TopMenuAction actions[] = {
            TopMenuAction::File,
            TopMenuAction::Edit,
            TopMenuAction::Search,
            TopMenuAction::Run,
            TopMenuAction::Compile,
            TopMenuAction::Git,
            TopMenuAction::Debug,
            TopMenuAction::Options,
            TopMenuAction::Window,
            TopMenuAction::Help
        };

        for (TopMenuAction action : actions) {
            int startX = 0;
            int endX = 0;
            if (TryGetTopMenuRegionX(action, startX, endX) && x >= startX && x <= endX) {
                return action;
            }
        }

        return TopMenuAction::None;
    }

    void HandleTopMenuAction(TopMenuAction action) {
        switch (action) {
        case TopMenuAction::File:
        case TopMenuAction::Edit:
        case TopMenuAction::Search:
        case TopMenuAction::Run:
        case TopMenuAction::Compile:
        case TopMenuAction::Git:
        case TopMenuAction::Debug:
        case TopMenuAction::Options:
        case TopMenuAction::Window:
        case TopMenuAction::Help:
            ShowMenuPopup(action);
            break;
        case TopMenuAction::None:
        default:
            break;
        }
    }

    void RenderEditorWindows() {
        if (m_documents.empty()) return;

        int activeIdx = m_activeDocIndex;

        // In Tabs mode, draw the tab strip across row 1 first.
        if (m_windowLayout == WindowLayout::Tabs) {
            RenderTabStrip();
        }

        for (int i = 0; i < static_cast<int>(m_documents.size()); ++i) {
            int topY = 0, botY = 0;
            if (!GetDocWindowFrame(i, topY, botY)) continue; // hidden in this layout
            RenderSingleWindow(m_documents[i], i == activeIdx, topY, botY);
        }
    }

    // Draw a horizontal tab bar of open documents on row 1 (Tabs layout).
    void RenderTabStrip() {
        for (int x = 0; x < m_width; ++x) {
            PutChar(x, 1, L' ', m_palette.MenuAttr);
        }
        int x = 1;
        for (int i = 0; i < static_cast<int>(m_documents.size()); ++i) {
            const auto& d = m_documents[i];
            std::string label = " " + d->Title + (d->IsModified ? "*" : "") + " ";
            WORD attr = (i == m_activeDocIndex) ? m_palette.MenuHotkeyAttr : m_palette.MenuAttr;
            DrawString(x, 1, label, attr);
            x += static_cast<int>(label.size()) + 1;
            if (x >= m_width - 1) break;
        }
    }

    void CycleWindowLayout() {
        switch (m_windowLayout) {
        case WindowLayout::Split: m_windowLayout = WindowLayout::Maximized; break;
        case WindowLayout::Maximized: m_windowLayout = WindowLayout::Tabs; break;
        case WindowLayout::Tabs: m_windowLayout = WindowLayout::Split; break;
        }
        const char* name = (m_windowLayout == WindowLayout::Split) ? "Split"
            : (m_windowLayout == WindowLayout::Maximized) ? "Maximized" : "Tabs";
        SetStatus(std::string("Window layout: ") + name);
        m_needsRender = true;
    }

    void RenderSingleWindow(std::shared_ptr<Document> doc, bool isActive, int topY, int botY) {
        WORD borderAttr = isActive ? m_palette.WinBorderActive : m_palette.WinBorderInactive;

        // Draw window frame in ASCII to avoid codepage artifacts on some terminals.
        PutChar(0, topY, '+', borderAttr);
        PutChar(m_width - 1, topY, '+', borderAttr);
        PutChar(0, botY, '+', borderAttr);
        PutChar(m_width - 1, botY, '+', borderAttr);

        for (int x = 1; x < m_width - 1; ++x) {
            PutChar(x, topY, '-', borderAttr);
            PutChar(x, botY, '-', borderAttr);
        }

        for (int y = topY + 1; y < botY; ++y) {
            PutChar(0, y, '|', borderAttr);
            PutChar(m_width - 1, y, '|', borderAttr);
            for (int x = 1; x < m_width - 1; ++x) {
                PutChar(x, y, L' ', m_palette.EditorBg);
            }
        }

        // Draw Window Title
        std::string titleStr = " " + std::to_string(doc->WindowId) + ": " + doc->Title + (doc->IsModified ? " *" : "") + " ";
        DrawString(3, topY, titleStr, m_palette.WinTitleAttr);

        // Draw Window Controls
        DrawString(m_width - 6, topY, "[X]", borderAttr);

        // Render Document Text Buffer with Syntax Highlighting Engine
        const auto& langCfg = m_langRegistry.GetConfig(doc->Lang);
        int viewHeight = botY - topY - 1;
        int lineNumWidth = 5;
        int textMaxLen = m_width - lineNumWidth - 3;

        // Horizontal scroll offset in DISPLAY CELLS (not bytes).
        int scrollX = (std::max)(0, doc->ScrollX);

        // Compute the lexical state at the start of the first visible line using
        // the per-document incremental cache. This rescans only from the first
        // edited line, so scrolling a huge file costs O(1) amortized per frame
        // instead of O(ScrollY).
        HighlightState lineState = doc->GetLineState(static_cast<size_t>(doc->ScrollY));

        for (int i = 0; i < viewHeight; ++i) {
            int lineIdx = doc->ScrollY + i;
            if (lineIdx >= (int)doc->Lines.size()) break;

            int drawY = topY + 1 + i;

            // Line numbers
            std::string lineNum = std::to_string(lineIdx + 1);
            while (lineNum.length() < 4) lineNum = " " + lineNum;
            DrawString(1, drawY, lineNum + "|", m_palette.LineNumAttr);

            // Text Highlighting: render only the visible window starting at the
            // display column ScrollX (converted to a codepoint-aligned byte offset).
            const std::string& fullLine = doc->Lines[lineIdx];
            std::string lineText;
            int startByte = DisplayColToByteOffset(fullLine, scrollX);
            if (startByte < static_cast<int>(fullLine.size())) {
                lineText = fullLine.substr(static_cast<size_t>(startByte));
            }
            // When scrolled horizontally into a continuing block comment/string,
            // the slice won't contain the opener, so seed the state for the slice.
            HighlightState sliceState = lineState;
            // Pass the slice's true starting display column so tab stops line up.
            int sliceStartCol = ByteOffsetToDisplayCol(fullLine, startByte);
            RenderHighlightedLine(1 + lineNumWidth, drawY, lineText, langCfg, textMaxLen, sliceState, sliceStartCol);
            // Carry the full-line end state to the next visible line.
            ScanLineState(fullLine, lineState);
        }

        // Window Status info line at bottom frame border
        std::string infoStr = " Line " + std::to_string(doc->CursorY + 1) + " Col " + std::to_string(doc->CursorX + 1) +
                              " [" + langCfg.Name + "] ";
        DrawString(m_width - (int)infoStr.length() - 3, botY, infoStr, borderAttr);

        // Render Cursor (visual column, offset by horizontal scroll).
        if (isActive) {
            int curDrawY = topY + 1 + (doc->CursorY - doc->ScrollY);
            const std::string& cursorLine = doc->Lines[doc->CursorY];
            int cursorCol = ByteOffsetToDisplayCol(cursorLine, doc->CursorX);
            int curDrawX = 1 + lineNumWidth + (cursorCol - scrollX);
            if (curDrawY > topY && curDrawY < botY && curDrawX >= 1 + lineNumWidth && curDrawX < m_width - 1) {
                SetCursorPositionVT(curDrawX, curDrawY);
            }
        }
    }

    // Advance `state` by scanning a whole line without rendering. Delegates to the
    // shared free scanner used by the per-document line-state cache.
    void ScanLineState(const std::string& line, HighlightState& state) {
        ScanHighlightLineState(line, state);
    }

    // Render one line with syntax highlighting. `state` is the lexical state at the
    // start of this (possibly horizontally-sliced) text and is updated in place.
    // Characters are decoded as UTF-8 codepoints and advance by their display width
    // (1 or 2 cells). `lineStartCol` is the display column where this slice begins
    // in the original line, so tabs expand to the correct stop even when scrolled.
    void RenderHighlightedLine(int startX, int y, const std::string& line, const LanguageConfig& cfg, int maxLen, HighlightState& state, int lineStartCol = 0) {
        int currentX = startX;
        int lineCol = lineStartCol; // display column within the original line
        // Draw one codepoint starting at byte `pos`; advances currentX by its width.
        auto draw = [&](size_t pos, int len, WORD attr) {
            int lenI = 1;
            uint32_t cp = DecodeUtf8Codepoint(line, pos, lenI);
            if (cp == '\t') {
                // Expand tab to the next multiple of kTabWidth using spaces, so the
                // buffer's raw \t never reaches the terminal and alignment is exact.
                int w = kTabWidth - (lineCol % kTabWidth);
                for (int k = 0; k < w && (currentX - startX) < maxLen; ++k) {
                    PutChar(currentX + k, y, L' ', attr);
                }
                currentX += w;
                lineCol += w;
                return;
            }
            wchar_t wc = L'?';
            if (len == 1) {
                wc = static_cast<wchar_t>(static_cast<unsigned char>(line[pos]));
            } else {
                wchar_t wbuf[4] = {0};
                if (MultiByteToWideChar(CP_UTF8, 0, line.data() + pos, len, wbuf, 3) > 0) {
                    wc = wbuf[0];
                }
            }
            int w = CodepointDisplayWidth(cp);
            if ((currentX - startX) < maxLen) PutChar(currentX, y, wc, attr);
            // Pad the trailing cell of a wide glyph so no stale char shows through.
            for (int k = 1; k < w && (currentX + k - startX) < maxLen; ++k) {
                PutChar(currentX + k, y, L' ', attr);
            }
            currentX += (w > 0 ? w : 1);
            lineCol += (w > 0 ? w : 1);
        };

        size_t idx = 0;
        const size_t n = line.length();
        while (idx < n) {
            int cpLen = 1;
            DecodeUtf8Codepoint(line, idx, cpLen);

            // Continuing block comment.
            if (state.inBlockComment) {
                size_t close = line.find("*/", idx);
                size_t end = (close == std::string::npos) ? n : close + 2;
                while (idx < end) {
                    int l = 1; DecodeUtf8Codepoint(line, idx, l);
                    draw(idx, l, m_palette.CommentAttr);
                    idx += static_cast<size_t>(l);
                }
                if (close != std::string::npos) state.inBlockComment = false;
                continue;
            }

            // Continuing string literal.
            if (state.stringQuote != '\0') {
                char q = state.stringQuote;
                char c = line[idx];
                draw(idx, cpLen, m_palette.StringAttr);
                if (c == '\\' && idx + 1 < n) {
                    int l2 = 1; DecodeUtf8Codepoint(line, idx + 1, l2);
                    draw(idx + 1, l2, m_palette.StringAttr);
                    idx += static_cast<size_t>(cpLen + l2);
                    continue;
                }
                if (c == q) state.stringQuote = '\0';
                idx += static_cast<size_t>(cpLen);
                continue;
            }

            // Line comments (// or #): consume to end.
            if ((line[idx] == '/' && idx + 1 < n && line[idx + 1] == '/') || line[idx] == '#') {
                while (idx < n) {
                    int l = 1; DecodeUtf8Codepoint(line, idx, l);
                    draw(idx, l, m_palette.CommentAttr);
                    idx += static_cast<size_t>(l);
                }
                break;
            }

            // Block comment open.
            if (line[idx] == '/' && idx + 1 < n && line[idx + 1] == '*') {
                state.inBlockComment = true;
                draw(idx, 1, m_palette.CommentAttr);
                draw(idx + 1, 1, m_palette.CommentAttr);
                idx += 2;
                continue;
            }

            // String open.
            if (line[idx] == '"' || line[idx] == '\'') {
                state.stringQuote = line[idx];
                draw(idx, cpLen, m_palette.StringAttr);
                idx += static_cast<size_t>(cpLen);
                continue;
            }

            // Identifier / Keyword / Number (ASCII fast path).
            if (isalnum((unsigned char)line[idx]) || line[idx] == '_') {
                std::string word;
                size_t wordStart = idx;
                while (idx < n && (isalnum((unsigned char)line[idx]) || line[idx] == '_')) {
                    word += line[idx++];
                }
                bool isKw = std::find(cfg.Keywords.begin(), cfg.Keywords.end(), word) != cfg.Keywords.end();
                WORD attr = isKw ? m_palette.KeywordAttr : (isdigit((unsigned char)word[0]) ? m_palette.NumberAttr : m_palette.TextDefault);
                size_t p = wordStart;
                while (p < idx) { draw(p, 1, attr); ++p; }
            } else {
                draw(idx, cpLen, m_palette.TextDefault);
                idx += static_cast<size_t>(cpLen);
            }
        }
    }

    void RenderStatusBar() {
        int statusY = m_height - 1;
        for (int x = 0; x < m_width; ++x) {
            PutChar(x, statusY, L' ', m_palette.StatusBarAttr);
        }

        std::string statusText = " CrossForge | F1 Help | F2 Save | F3 Open | F4 Diagnostics | F8 Terminal | F9 Build | F10 Run | Ctrl+Z Undo | Alt+X Exit";
        if (!m_statusMessage.empty()) {
            statusText += " | " + m_statusMessage;
        }
        if ((int)statusText.size() > m_width) {
            statusText = statusText.substr(0, m_width);
        }
        DrawString(0, statusY, statusText, m_palette.StatusBarAttr);
    }

    // ========================================================================
    // HELP SYSTEM & DIALOGS
    // ========================================================================

    std::string GetWindowsVersionText() const {
        using RtlGetVersionPtr = LONG(WINAPI*)(OSVERSIONINFOEXW*);
        HMODULE hNtDll = GetModuleHandleA("ntdll.dll");
        if (!hNtDll) return "Windows (version unknown)";

        auto rtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(hNtDll, "RtlGetVersion"));
        if (!rtlGetVersion) return "Windows (version unknown)";

        OSVERSIONINFOEXW info{};
        info.dwOSVersionInfoSize = sizeof(info);
        if (rtlGetVersion(&info) != 0) {
            return "Windows (version unknown)";
        }

        std::string family;
        if (info.dwMajorVersion == 10 && info.dwBuildNumber >= 22000) {
            family = "Windows 11";
        } else if (info.dwMajorVersion == 10) {
            family = "Windows 10";
        } else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 3) {
            family = "Windows 8.1";
        } else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 2) {
            family = "Windows 8";
        } else if (info.dwMajorVersion == 6 && info.dwMinorVersion == 1) {
            family = "Windows 7";
        } else {
            family = "Windows";
        }

        return family + " " + std::to_string(info.dwMajorVersion) + "." + std::to_string(info.dwMinorVersion) +
               " (Build " + std::to_string(info.dwBuildNumber) + ")";
    }

    void ShowHelpDialog() {
        int dWidth = m_width;
        int dHeight = m_height;
        int startX = 0;
        int startY = 0;

        WORD dAttr = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE; // Light Grey Box
        bool prevCursorVisible = SetCursorVisible(false);

        std::string osLine = "Detected OS: " + GetWindowsVersionText();
        std::vector<std::string> helpLines = {
            osLine,
            "",
            "MAIN WORKFLOW:",
            "  1) Open file (F3 or Ctrl+O)",
            "  2) Edit text in active pane",
            "  3) Save (F2 or Ctrl+S)",
            "  4) Build/Run/Test from Compile menu or Ctrl+B/Ctrl+R/Ctrl+T",
            "",
            "KEYBOARD SHORTCUTS:",
            "  F1 Help, F2 Save, F3 Open, F4 IDE Diagnostics, F8 Terminal, F9 Build, F10 Run",
            "  Ctrl+Alt+D Debug Current",
            "  Ctrl+N New, Ctrl+W Close Window, Ctrl+E Next Window",
            "  Ctrl+F Find, Ctrl+G Next Find, Ctrl+Shift+G Previous Find, Ctrl+H Replace",
            "  Ctrl+Z Undo, Ctrl+Y Redo, Ctrl+E Open Most Recent File",
            "  Ctrl+Shift+C/Ctrl+X/Ctrl+V/Ctrl+A Copy/Cut/Paste/Select All",
            "  Ctrl+L Linker Command, Ctrl+Shift+D Debugger Detection",
            "  Alt+X Exit",
            "",
            "MENUS OVERVIEW:",
            "  File    : New/Open/Save/Save As/Close/Exit",
            "  Edit    : Clipboard + line operations",
            "  Search  : Find/Replace",
            "  View    : Windows Terminal, WSL Terminal, SSH Terminal, compiler output",
            "  Compile : Build/Run/Test + compiler detection",
            "  Debug   : Debug current target + output + debugger detection",
            "  Options : Theme, Add Compiler Path, Run Linker Command",
            "  Window  : Next/New and clickable [X] close per pane",
            "  Help    : Help topics, IDE diagnostics, and About",
            "",
            "TOOLCHAIN NOTES:",
            "  - Compiler and debugger detection uses current PATH/session.",
            "  - Rust (.rs) uses rustc; Go (.go) uses go build when available.",
            "  - WSL Terminal uses the configured default WSL distribution.",
            "  - SSH Terminal requires OpenSSH ssh.exe and a user@host target.",
            "  - Build and Link output is appended to %USERPROFILE%\\.crossforge_compiler_output.",
            "  - Add Compiler Path updates PATH for this running session.",
            "  - Linker command attempts language-appropriate linker/toolchain.",
            "  - Press ESC while Build/Run/Test is active to cancel the process.",
            "  - Session files, theme, tabs, and recent files persist in %USERPROFILE%\\.crossforge.",
            "  - Sandbox timeout env: crossforge_PROCESS_TIMEOUT_MS (ms, default 180000).",
            "  - Sandbox child limit env: crossforge_PROCESS_MAX_CHILDREN (default 64).",
            "",
            
        };

        int firstLineY = startY + 4;
        int lastContentY = dHeight - 4;
        int contentRows = (std::max)(1, lastContentY - firstLineY + 1);
        int maxScroll = (std::max)(0, static_cast<int>(helpLines.size()) - contentRows);
        int scrollOffset = 0;
        bool dirty = true;

        while (true) {
            if (dirty) {
                for (int y = startY; y < startY + dHeight; ++y) {
                    for (int x = startX; x < startX + dWidth; ++x) {
                        PutChar(x, y, L' ', dAttr);
                    }
                }

                DrawString(startX + 2, startY + 1, "CrossForge Help System - Developer IDE for Windows command shells", dAttr | FOREGROUND_RED);
                DrawString(startX + 2, startY + 2, "=============================================================", dAttr);

                for (int row = 0; row < contentRows; ++row) {
                    int idx = scrollOffset + row;
                    if (idx >= static_cast<int>(helpLines.size())) break;
                    DrawString(startX + 2, firstLineY + row, helpLines[idx], dAttr);
                }

                std::string navLine = "Use Mouse Wheel / Up / Down / PgUp / PgDn / Home / End to navigate";
                if ((int)navLine.size() > dWidth - 4) navLine = navLine.substr(0, dWidth - 4);
                DrawString(startX + 2, dHeight - 3, navLine, dAttr | FOREGROUND_BLUE);

                std::string posLine = "Line " + std::to_string(scrollOffset + 1) + "-" +
                                      std::to_string((std::min)(scrollOffset + contentRows, static_cast<int>(helpLines.size()))) +
                                      " of " + std::to_string(helpLines.size()) + " | ESC/ENTER Close";
                if ((int)posLine.size() > dWidth - 4) posLine = posLine.substr(0, dWidth - 4);
                DrawString(startX + 2, dHeight - 2, posLine, dAttr);

                FlushBuffer();
                dirty = false;
            }

            INPUT_RECORD ir;
            DWORD read;
            ReadInputEvent(ir);

            if (ir.EventType == WINDOW_BUFFER_SIZE_EVENT) {
                UpdateBufferSize();
                dWidth = m_width;
                dHeight = m_height;
                firstLineY = startY + 4;
                lastContentY = dHeight - 4;
                contentRows = (std::max)(1, lastContentY - firstLineY + 1);
                maxScroll = (std::max)(0, static_cast<int>(helpLines.size()) - contentRows);
                if (scrollOffset > maxScroll) scrollOffset = maxScroll;
                dirty = true;
                continue;
            }

            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
                if (vk == VK_ESCAPE || vk == VK_RETURN) {
                    break;
                }

                int next = scrollOffset;
                if (vk == VK_DOWN) {
                    next = (std::min)(maxScroll, scrollOffset + 1);
                } else if (vk == VK_UP) {
                    next = (std::max)(0, scrollOffset - 1);
                } else if (vk == VK_NEXT) {
                    next = (std::min)(maxScroll, scrollOffset + contentRows);
                } else if (vk == VK_PRIOR) {
                    next = (std::max)(0, scrollOffset - contentRows);
                } else if (vk == VK_HOME) {
                    next = 0;
                } else if (vk == VK_END) {
                    next = maxScroll;
                }

                if (next != scrollOffset) {
                    scrollOffset = next;
                    dirty = true;
                }
            } else if (ir.EventType == MOUSE_EVENT) {
                DWORD flags = ir.Event.MouseEvent.dwEventFlags;
                DWORD btn = ir.Event.MouseEvent.dwButtonState;
                if (flags == MOUSE_WHEELED) {
                    int wheel = static_cast<SHORT>(HIWORD(btn));
                    int next = scrollOffset + ((wheel > 0) ? -3 : 3);
                    next = (std::max)(0, (std::min)(next, maxScroll));
                    if (next != scrollOffset) {
                        scrollOffset = next;
                        dirty = true;
                    }
                }
            }
        }

        SetCursorVisible(prevCursorVisible);
        m_needsRender = true;
    }

    void ShowAboutDialog() {
        int dWidth = 64;
        int dHeight = 12;
        int startX = (m_width - dWidth) / 2;
        int startY = (m_height - dHeight) / 2;

        WORD dAttr = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;

        for (int y = startY; y < startY + dHeight; ++y) {
            for (int x = startX; x < startX + dWidth; ++x) {
                PutChar(x, y, L' ', dAttr);
            }
        }

        DrawString(startX + 2, startY + 1, "About CrossForge", dAttr | FOREGROUND_RED);
        DrawString(startX + 2, startY + 2, "========================================================", dAttr);
        DrawString(startX + 2, startY + 4, APP_TITLE, dAttr);
        DrawString(startX + 2, startY + 5, COPYRIGHT_NOTICE, dAttr);
        DrawString(startX + 2, startY + 7, "Developer IDE for modern Windows consoles.", dAttr);
        DrawString(startX + 2, startY + 9, "Press ESC or ENTER to close.", dAttr);
        FlushBuffer();

        INPUT_RECORD ir;
        DWORD read;
        while (true) {
            ReadInputEvent(ir);
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                if (ir.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE || ir.Event.KeyEvent.wVirtualKeyCode == VK_RETURN) {
                    break;
                }
            }
        }

        m_needsRender = true;
    }

    void ShowCompilerDetectDialog() {
        auto detected = CompilerDetector::DetectAll();
        int contentWidth = 0;
        for (const auto& tool : detected) {
            std::string status = tool.Found ? "[FOUND] " + tool.Path : "[NOT FOUND]";
            std::string line = tool.Name + " : " + status;
            contentWidth = (std::max)(contentWidth, static_cast<int>(line.size()));
        }

        int dWidth = (std::max)(78, contentWidth + 6);
        dWidth = (std::min)(dWidth, m_width - 4);

        int availableRows = (std::max)(1, m_height - 11);
        int visibleRows = (std::min)(availableRows, static_cast<int>(detected.size()));
        int dHeight = (std::max)(12, visibleRows + 7);
        dHeight = (std::min)(dHeight, m_height - 4);
        int startX = (m_width - dWidth) / 2;
        int startY = (m_height - dHeight) / 2;

        WORD dAttr = BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE;

        for (int y = startY; y < startY + dHeight; ++y) {
            for (int x = startX; x < startX + dWidth; ++x) {
                PutChar(x, y, L' ', dAttr);
            }
        }

        DrawString(startX + 2, startY + 1, "CrossForge Auto-Detected Compilers & Build Tools", dAttr | FOREGROUND_RED);
        DrawString(startX + 2, startY + 2, "=========================================================", dAttr);

        int lineMaxLen = dWidth - 4;
        int maxRows = (std::min)(visibleRows, dHeight - 6);
        for (int i = 0; i < maxRows; ++i) {
            std::string status = detected[i].Found ? "[FOUND] " + detected[i].Path : "[NOT FOUND]";
            std::string line = detected[i].Name + " : " + status;
            if ((int)line.size() > lineMaxLen) {
                if (lineMaxLen > 3) line = line.substr(0, lineMaxLen - 3) + "...";
                else line = line.substr(0, lineMaxLen);
            }
            DrawString(startX + 2, startY + 4 + i, line, dAttr);
        }

        DrawString(startX + 2, startY + dHeight - 2, "Press ESC or ENTER to close...", dAttr);
        FlushBuffer();

        INPUT_RECORD ir;
        DWORD read;
        while (true) {
            ReadInputEvent(ir);
            if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
                if (ir.Event.KeyEvent.wVirtualKeyCode == VK_ESCAPE || ir.Event.KeyEvent.wVirtualKeyCode == VK_RETURN) {
                    break;
                }
            }
        }

        m_needsRender = true;
    }

    // ========================================================================
    // TERMINAL INTEGRATION (F8 ENGINE)
    // ========================================================================

    void LaunchInteractiveTerminal(const std::string& title, const std::string& commandLine) {
        std::cout << "\x1b[2J\x1b[H";
        std::cout << "\n";
        std::cout << " CrossForge " << title << "\n";
        std::cout << " Copyright (C) 2026, Roberto J. Dohnert\n";
        std::cout << " Type 'exit' and press ENTER to return to CrossForge UI.\n";
        std::cout << "\n\n";

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};

        // Interactive shells must inherit ordinary console input semantics;
        // the editor's raw VT/mouse mode makes WSL and SSH misread input.
        FlushConsoleInputBuffer(m_hStdIn);
        SetConsoleMode(m_hStdIn, m_defaultInputMode);
        SetConsoleMode(m_hStdOut, m_defaultOutputMode);

        std::wstring commandLineW = Utf8ToWide(commandLine);
        std::vector<wchar_t> cmdBuf(commandLineW.begin(), commandLineW.end());
        cmdBuf.push_back(L'\0');

        BOOL launched = CreateProcessW(
            nullptr, cmdBuf.data(), NULL, NULL, FALSE,
            0, NULL, NULL, &si, &pi
        );

        if (launched) {
            WaitForSingleObject(pi.hProcess, INFINITE);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        } else {
            SetStatus("Failed to launch terminal shell.");
        }

        // Restore TUI Environment
        SetConsoleOutputCP(CP_UTF8);
        SetConsoleCP(CP_UTF8);
        ApplyEditorConsoleModes();
        FlushConsoleInputBuffer(m_hStdIn);
        m_needsRender = true;
    }

    void LaunchTerminalIntegration() {
        char sysDir[MAX_PATH] = {0};
        std::string comSpec = "C:\\Windows\\System32\\cmd.exe";
        UINT sysLen = GetSystemDirectoryA(sysDir, MAX_PATH);
        if (sysLen > 0 && sysLen < MAX_PATH) {
            comSpec = std::string(sysDir, sysLen) + "\\cmd.exe";
        }
        LaunchInteractiveTerminal("Terminal Shell Environment Integrator", "\"" + comSpec + "\"");
    }

    void LaunchWslTerminal() {
        LaunchInteractiveTerminal("WSL Terminal", "wsl.exe");
    }

    void LaunchSshTerminal() {
        std::string target = PromptLine("SSH target (user@host):", "SSH Terminal");
        if (target.empty()) {
            SetStatus("SSH terminal cancelled.");
            return;
        }

        // Whitelist only safe user@host characters: letters, digits, '.', '-', '_',
        // '@', ':', and '[]' for IPv6 literals. Blocks ; | & ` $ ( ) < > quotes, etc.
        auto isSafeTarget = [](const std::string& t) {
            if (t.empty() || t.front() == '-' || t.size() > 253) return false;
            for (char c : t) {
                bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                          (c >= '0' && c <= '9') || c == '.' || c == '-' || c == '_' ||
                          c == '@' || c == ':' || c == '[' || c == ']';
                if (!ok) return false;
            }
            return true;
        };
        if (!isSafeTarget(target)) {
            SetStatus("SSH target contains invalid characters.");
            return;
        }

        LaunchInteractiveTerminal("SSH Terminal", "ssh.exe \"" + target + "\"");
    }

    // ========================================================================
    // INPUT DISPATCHER
    // ========================================================================

    void ProcessInput() {
        if (WaitForSingleObject(m_hStdIn, 33) != WAIT_OBJECT_0 && m_inPos >= m_inBuf.size()) {
            return;
        }

        INPUT_RECORD ir;
        if (!ReadInputEvent(ir)) {
            return;
        }

        if (ir.EventType == WINDOW_BUFFER_SIZE_EVENT) {
            m_needsRender = true;
            return;
        }

        if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
            WORD vk = ir.Event.KeyEvent.wVirtualKeyCode;
            DWORD state = ir.Event.KeyEvent.dwControlKeyState;

            // Dedicated function-key accelerators retained for historical flow.
            if (vk == VK_F2) {
                ExecuteMenuCommand(MenuCommand::FileSave);
                return;
            }
            if (vk == VK_F3) {
                ExecuteMenuCommand(MenuCommand::FileOpen);
                return;
            }

            // All menu command combos are centralized here.
            if (HandleMenuAccelerator(vk, state)) {
                m_needsRender = true;
                return;
            }

            // Active Document Typing Logic
            if (!m_documents.empty()) {
                auto doc = m_documents[m_activeDocIndex];
                EnsureDocumentInvariants(doc);
                if (vk == VK_UP) {
                    int preferredX = doc->CursorX;
                    if (doc->CursorY > 0) {
                        doc->CursorY--;
                        doc->CursorX = (std::min)(preferredX, static_cast<int>(doc->Lines[doc->CursorY].size()));
                    }
                    m_selectAllActive = false;
                } else if (vk == VK_DOWN) {
                    int preferredX = doc->CursorX;
                    if (doc->CursorY < (int)doc->Lines.size() - 1) {
                        doc->CursorY++;
                        doc->CursorX = (std::min)(preferredX, static_cast<int>(doc->Lines[doc->CursorY].size()));
                    }
                    m_selectAllActive = false;
                } else if (vk == VK_LEFT) {
                    if (doc->CursorX > 0) {
                        doc->CursorX = static_cast<int>(PrevUtf8CharStart(doc->Lines[doc->CursorY], static_cast<size_t>(doc->CursorX)));
                    } else if (doc->CursorY > 0) {
                        doc->CursorY--;
                        doc->CursorX = static_cast<int>(doc->Lines[doc->CursorY].size());
                    }
                    m_selectAllActive = false;
                } else if (vk == VK_RIGHT) {
                    int lineLen = static_cast<int>(doc->Lines[doc->CursorY].length());
                    if (doc->CursorX < lineLen) {
                        doc->CursorX = static_cast<int>(NextUtf8CharEnd(doc->Lines[doc->CursorY], static_cast<size_t>(doc->CursorX)));
                    } else if (doc->CursorY < static_cast<int>(doc->Lines.size()) - 1) {
                        doc->CursorY++;
                        doc->CursorX = 0;
                    }
                    m_selectAllActive = false;
                } else if (vk == VK_HOME) {
                    // Home: start of line; Ctrl+Home: start of document.
                    if (IsCtrlPressed(state)) { doc->CursorY = 0; }
                    doc->CursorX = 0;
                    m_selectAllActive = false;
                } else if (vk == VK_END) {
                    // End: end of line; Ctrl+End: end of document.
                    if (IsCtrlPressed(state)) { doc->CursorY = static_cast<int>(doc->Lines.size()) - 1; }
                    doc->CursorX = static_cast<int>(doc->Lines[doc->CursorY].size());
                    m_selectAllActive = false;
                } else if (vk == VK_PRIOR) { // Page Up
                    int page = (std::max)(1, GetDocumentViewHeight(m_activeDocIndex) - 1);
                    doc->CursorY = (std::max)(0, doc->CursorY - page);
                    doc->CursorX = (std::min)(doc->CursorX, static_cast<int>(doc->Lines[doc->CursorY].size()));
                    m_selectAllActive = false;
                } else if (vk == VK_NEXT) {  // Page Down
                    int page = (std::max)(1, GetDocumentViewHeight(m_activeDocIndex) - 1);
                    doc->CursorY = (std::min)(static_cast<int>(doc->Lines.size()) - 1, doc->CursorY + page);
                    doc->CursorX = (std::min)(doc->CursorX, static_cast<int>(doc->Lines[doc->CursorY].size()));
                    m_selectAllActive = false;
                } else if (vk == VK_RETURN) {
                    doc->RecordEdit();
                    doc->InvalidateLineStatesFrom(static_cast<size_t>(doc->CursorY));
                    std::string line = doc->Lines[doc->CursorY];
                    std::string remainder = line.substr(doc->CursorX);
                    doc->Lines.set(doc->CursorY, line.substr(0, doc->CursorX));
                    doc->Lines.insert(doc->CursorY + 1, remainder);
                    doc->CursorY++;
                    doc->CursorX = 0;
                    doc->IsModified = true;
                    m_selectAllActive = false;
                } else if (vk == VK_BACK) {
                    if (doc->CursorX > 0) {
                        doc->RecordEdit();
                        doc->InvalidateLineStatesFrom(static_cast<size_t>(doc->CursorY));
                        size_t charStart = PrevUtf8CharStart(doc->Lines[doc->CursorY], static_cast<size_t>(doc->CursorX));
                        std::string cur = doc->Lines[doc->CursorY];
                        cur.erase(charStart, static_cast<size_t>(doc->CursorX) - charStart);
                        doc->Lines.set(doc->CursorY, std::move(cur));
                        doc->CursorX = static_cast<int>(charStart);
                        doc->IsModified = true;
                        m_selectAllActive = false;
                    } else if (doc->CursorY > 0) {
                        doc->RecordEdit();
                        doc->InvalidateLineStatesFrom(static_cast<size_t>(doc->CursorY - 1));
                        int previousLineLength = static_cast<int>(doc->Lines[doc->CursorY - 1].size());
                        doc->Lines.set(doc->CursorY - 1, doc->Lines[doc->CursorY - 1] + doc->Lines[doc->CursorY]);
                        doc->Lines.erase(doc->CursorY);
                        doc->CursorY--;
                        doc->CursorX = previousLineLength;
                        doc->IsModified = true;
                        m_selectAllActive = false;
                    }
                } else {
                    wchar_t wch = ir.Event.KeyEvent.uChar.UnicodeChar;
                    std::string chUtf8 = Utf8FromWideChar(wch);
                    if (!chUtf8.empty()) {
                        doc->RecordEdit();
                        doc->InvalidateLineStatesFrom(static_cast<size_t>(doc->CursorY));
                        std::string cur = doc->Lines[doc->CursorY];
                        cur.insert(doc->CursorX, chUtf8);
                        doc->Lines.set(doc->CursorY, std::move(cur));
                        doc->CursorX += static_cast<int>(chUtf8.size());
                        doc->IsModified = true;
                        m_selectAllActive = false;
                    }
                }
                EnsureDocumentInvariants(doc);
                EnsureCursorVisible(doc, m_activeDocIndex);
                m_needsRender = true;
            }
        } else if (ir.EventType == MOUSE_EVENT) {
            COORD pos = ir.Event.MouseEvent.dwMousePosition;
            DWORD btn = ir.Event.MouseEvent.dwButtonState;
            DWORD flags = ir.Event.MouseEvent.dwEventFlags;

            // Mouse wheel scrolling
            if (flags == MOUSE_WHEELED && !m_documents.empty()) {
                auto doc = m_documents[m_activeDocIndex];
                EnsureDocumentInvariants(doc);
                int wheel = static_cast<SHORT>(HIWORD(btn));
                int step = (wheel > 0) ? -3 : 3;
                int maxScroll = (std::max)(0, static_cast<int>(doc->Lines.size()) - 1);
                doc->ScrollY = (std::max)(0, (std::min)(doc->ScrollY + step, maxScroll));
                if (doc->CursorY < doc->ScrollY) doc->CursorY = doc->ScrollY;
                SetStatus("Scrolled.");
                return;
            }

            // Top menu mouse support with press highlight.
            if ((btn & FROM_LEFT_1ST_BUTTON_PRESSED) != 0 && flags == 0 && pos.Y == 0) {
                TopMenuAction action = HitTestTopMenu(pos.X);
                if (action != TopMenuAction::None) {
                    m_pressedTopMenuAction = action;
                    HandleTopMenuAction(action);
                    m_pressedTopMenuAction = TopMenuAction::None;
                    m_needsRender = true;
                    return;
                }
            }

            // Clickable [X] close button per editor window.
            if ((btn & FROM_LEFT_1ST_BUTTON_PRESSED) != 0 && flags == 0) {
                int closeIdx = HitTestWindowCloseButton(pos.X, pos.Y);
                if (closeIdx >= 0) {
                    CloseDocumentAtIndex(closeIdx);
                    m_needsRender = true;
                    return;
                }
            }

            // Tab-strip click switches the active document (Tabs layout).
            if ((btn & FROM_LEFT_1ST_BUTTON_PRESSED) != 0 && flags == 0) {
                int tabIdx = HitTestTabStrip(pos.X, pos.Y);
                if (tabIdx >= 0 && tabIdx != m_activeDocIndex) {
                    m_activeDocIndex = tabIdx;
                    SetStatus("Switched window.");
                    m_needsRender = true;
                    return;
                }
            }

            // Text-area click sets caret location.
            if ((btn & FROM_LEFT_1ST_BUTTON_PRESSED) != 0 && flags == 0 && !m_documents.empty()) {
                auto doc = m_documents[m_activeDocIndex];
                int windowTop = 1;
                int windowBottom = m_height - 2;
                if (pos.Y > windowTop && pos.Y < windowBottom && pos.X > 5 && pos.X < m_width - 1) {
                    int lineIdx = doc->ScrollY + (pos.Y - (windowTop + 1));
                    lineIdx = (std::max)(0, (std::min)(lineIdx, static_cast<int>(doc->Lines.size()) - 1));
                    // Convert the clicked screen column (plus horizontal scroll) into
                    // a byte offset, accounting for multi-byte/wide characters.
                    int targetCol = (std::max)(0, pos.X - 6) + (std::max)(0, doc->ScrollX);
                    int colIdx = 0;
                    if (lineIdx < static_cast<int>(doc->Lines.size())) {
                        colIdx = DisplayColToByteOffset(doc->Lines[lineIdx], targetCol);
                        colIdx = (std::min)(colIdx, static_cast<int>(doc->Lines[lineIdx].size()));
                    }
                    doc->CursorY = lineIdx;
                    doc->CursorX = colIdx;
                    EnsureDocumentInvariants(doc);
                    EnsureCursorVisible(doc, m_activeDocIndex);
                    SetStatus("Cursor moved.");
                    m_needsRender = true;
                }
            }
        }
    }
};

// ============================================================================
// ENTRY POINT
// ============================================================================

int main() {
    try {
        CrossForgeEngine engine;
        engine.Run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal Exception in CrossForge Engine: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}