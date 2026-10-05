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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <regex>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <unordered_set>
#include <optional>
#include <io.h>
#include <cstdio>
#include <memory>

namespace fs = std::filesystem;

// ============================================================================
// 1. ANSI COLORS & DATA MODELS
// ============================================================================

namespace Color {
    const std::string RESET     = "\033[0m";
    const std::string BOLD      = "\033[1m";
    const std::string BLUE      = "\033[1;34m";
    const std::string GREEN     = "\033[1;32m";
    const std::string CYAN      = "\033[1;36m";
    const std::string MAGENTA   = "\033[1;35m";
    const std::string RED       = "\033[1;31m";
    const std::string YELLOW    = "\033[1;33m";
    const std::string GRAY      = "\033[90m";
}

enum class CaseMode {
    Smart,
    Sensitive,
    Insensitive
};

enum class OutputFormat {
    Default = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

struct CompiledPattern {
    std::string rawPattern;
    bool isFixed = false;
    bool caseSensitive = false;
    std::optional<std::regex> regexObj;
};

const std::unordered_set<std::string> DEFAULT_IGNORED_DIRS = {
    ".git", "node_modules", ".vs", "bin", "obj", "target", "vendor", "build", "dist", ".idea", ".vscode"
};

// ============================================================================
// 2. CONSOLE ENVIRONMENT & HELPERS
// ============================================================================

class ConsoleEnvironment {
public:
    static bool InitConsole() {
        SetConsoleOutputCP(CP_UTF8);
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return false;

        DWORD dwMode = 0;
        if (!GetConsoleMode(hOut, &dwMode)) return false;
        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        return SetConsoleMode(hOut, dwMode) != 0;
    }

    static bool ShouldUseColorByDefault() {
        if (!_isatty(_fileno(stdout))) return false;

        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return false;

        DWORD mode = 0;
        return GetConsoleMode(hOut, &mode) != 0;
    }
};

class PathHelper {
public:
    static std::string ToLower(const std::string& input) {
        std::string result = input;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    static std::wstring Utf8ToWide(const std::string& s) {
        if (s.empty()) return std::wstring();
        int count = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
        if (count <= 0) return std::wstring();
        std::wstring out(static_cast<size_t>(count), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, out.data(), count);
        if (!out.empty() && out.back() == L'\0') {
            out.pop_back();
        }
        return out;
    }

    static std::string PathToUtf8(const fs::path& p) {
        auto u8 = p.u8string();
        return std::string(u8.begin(), u8.end());
    }

    static bool IsHidden(const fs::directory_entry& entry) {
        std::wstring wpath = entry.path().wstring();
        DWORD attrs = GetFileAttributesW(wpath.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_HIDDEN)) {
            return true;
        }
        std::wstring filename = entry.path().filename().wstring();
        return (!filename.empty() && filename[0] == L'.');
    }

    static bool IsExecutable(const fs::directory_entry& entry) {
        if (!entry.is_regular_file()) return false;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        return (ext == ".exe" || ext == ".cmd" || ext == ".bat" || ext == ".com" || ext == ".ps1");
    }

    static bool IsIgnoredDirectory(const std::string& dirName, bool includeIgnored) {
        if (includeIgnored) return false;
        return DEFAULT_IGNORED_DIRS.find(ToLower(dirName)) != DEFAULT_IGNORED_DIRS.end();
    }

    static std::string QuoteForCreateProcessArg(const std::string& arg) {
        std::string out;
        out.push_back('"');

        size_t backslashes = 0;
        for (char c : arg) {
            if (c == '\\') {
                backslashes++;
                continue;
            }

            if (c == '"') {
                out.append(backslashes * 2 + 1, '\\');
                out.push_back('"');
                backslashes = 0;
                continue;
            }

            if (backslashes > 0) {
                out.append(backslashes, '\\');
                backslashes = 0;
            }
            out.push_back(c);
        }

        if (backslashes > 0) {
            out.append(backslashes * 2, '\\');
        }

        out.push_back('"');
        return out;
    }

    static std::string QuotePath(const std::string& pathStr) {
        return QuoteForCreateProcessArg(pathStr);
    }

    static std::string FormatPath(const fs::directory_entry& entry, const std::string& pathStr, bool useColor) {
        if (!useColor) return pathStr;

        if (entry.is_directory()) {
            return Color::CYAN + pathStr + Color::RESET;
        } else if (entry.is_symlink()) {
            return Color::MAGENTA + pathStr + Color::RESET;
        } else if (IsExecutable(entry)) {
            return Color::GREEN + pathStr + Color::RESET;
        }
        return pathStr;
    }
};

// ============================================================================
// 3. PATTERN MATCHER & COMMAND EXECUTOR
// ============================================================================

class CompiledPatternMatcher {
public:
    static bool IsCaseSensitive(const std::string& pattern, CaseMode mode) {
        if (mode == CaseMode::Sensitive) return true;
        if (mode == CaseMode::Insensitive) return false;
        return std::any_of(pattern.begin(), pattern.end(), [](unsigned char c) { return std::isupper(c); });
    }

    static CompiledPattern PreparePattern(const std::string& pattern, CaseMode mode, bool fixed) {
        CompiledPattern cp;
        cp.rawPattern = pattern;
        cp.isFixed = fixed;
        cp.caseSensitive = IsCaseSensitive(pattern, mode);

        if (!fixed && !pattern.empty()) {
            try {
                auto flags = std::regex_constants::ECMAScript;
                if (!cp.caseSensitive) flags |= std::regex_constants::icase;
                cp.regexObj = std::regex(pattern, flags);
            } catch (const std::regex_error&) {
                cp.isFixed = true;
            }
        }
        return cp;
    }

    static bool Matches(const std::string& filename, const CompiledPattern& cp) {
        if (cp.rawPattern.empty()) return true;

        if (cp.isFixed || !cp.regexObj.has_value()) {
            if (cp.caseSensitive) {
                return filename.find(cp.rawPattern) != std::string::npos;
            } else {
                return PathHelper::ToLower(filename).find(PathHelper::ToLower(cp.rawPattern)) != std::string::npos;
            }
        } else {
            return std::regex_search(filename, cp.regexObj.value());
        }
    }
};

class CommandExecutor {
public:
    static bool ExecuteCommandDirect(const std::string& commandLineUtf8) {
        std::wstring cmdLine = PathHelper::Utf8ToWide(commandLineUtf8);
        if (cmdLine.empty()) return false;

        STARTUPINFOW si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};

        std::wstring mutableCmd = cmdLine;
        BOOL ok = CreateProcessW(
            nullptr,
            mutableCmd.data(),
            nullptr,
            nullptr,
            FALSE,
            0,
            nullptr,
            nullptr,
            &si,
            &pi
        );

        if (!ok) {
            return false;
        }

        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }

    static std::string BuildExecCommand(const std::string& templateCmd, const fs::path& p) {
        std::string result = templateCmd;
        std::string fullPath = PathHelper::QuotePath(PathHelper::PathToUtf8(p));
        std::string fileName = PathHelper::QuotePath(PathHelper::PathToUtf8(p.filename()));
        std::string parentDir = PathHelper::QuotePath(PathHelper::PathToUtf8(p.parent_path()));
        std::string pathNoExt = PathHelper::QuotePath(PathHelper::PathToUtf8(p.parent_path() / p.stem()));

        auto replaceAll = [](std::string& str, const std::string& from, const std::string& to) {
            size_t start_pos = 0;
            while ((start_pos = str.find(from, start_pos)) != std::string::npos) {
                str.replace(start_pos, from.length(), to);
                start_pos += to.length();
            }
        };

        replaceAll(result, "{//}", parentDir);
        replaceAll(result, "{/}", fileName);
        replaceAll(result, "{.}", pathNoExt);
        replaceAll(result, "{}", fullPath);

        return result;
    }
};

// ============================================================================
// 4. OPTIONS & REPORTER
// ============================================================================

class FdOptions {
public:
    std::string pattern;
    std::vector<std::string> searchPaths;
    bool includeHidden = false;
    bool includeIgnored = false;
    CaseMode caseMode = CaseMode::Smart;
    bool fixedStrings = false;
    bool absolutePaths = false;
    bool print0 = false;
    bool colorOutput = true;
    bool colorOutputExplicit = false;
    int maxDepth = -1;
    int minDepth = -1;
    std::vector<std::string> extensions;
    std::vector<char> fileTypes;
    std::string execCommand;
    OutputFormat outputFormat = OutputFormat::Default;
    std::string pipeCommand;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                showHelp = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                showVersion = true;
                return true;
            } else if (arg == "--json") {
                outputFormat = OutputFormat::Json;
            } else if (arg == "--csv") {
                outputFormat = OutputFormat::Csv;
            } else if (arg == "--table") {
                outputFormat = OutputFormat::Table;
            } else if (arg == "--pipe" && i + 1 < argc) {
                pipeCommand = argv[++i];
            } else if (arg == "-H" || arg == "--hidden") {
                includeHidden = true;
            } else if (arg == "-I" || arg == "--no-ignore") {
                includeIgnored = true;
            } else if (arg == "-s" || arg == "--case-sensitive") {
                caseMode = CaseMode::Sensitive;
            } else if (arg == "-i" || arg == "--ignore-case") {
                caseMode = CaseMode::Insensitive;
            } else if (arg == "-F" || arg == "--fixed-strings") {
                fixedStrings = true;
            } else if (arg == "-a" || arg == "--absolute") {
                absolutePaths = true;
            } else if (arg == "-0" || arg == "--print0") {
                print0 = true;
            } else if (arg == "-d" || arg == "--max-depth") {
                if (i + 1 < argc) maxDepth = std::atoi(argv[++i]);
            } else if (arg.rfind("--max-depth=", 0) == 0) {
                maxDepth = std::atoi(arg.substr(12).c_str());
            } else if (arg == "--min-depth") {
                if (i + 1 < argc) minDepth = std::atoi(argv[++i]);
            } else if (arg.rfind("--min-depth=", 0) == 0) {
                minDepth = std::atoi(arg.substr(12).c_str());
            } else if (arg == "-e" || arg == "--extension") {
                if (i + 1 < argc) extensions.push_back(argv[++i]);
            } else if (arg == "-t" || arg == "--type") {
                if (i + 1 < argc) {
                    std::string t = argv[++i];
                    if (t == "f" || t == "file") fileTypes.push_back('f');
                    else if (t == "d" || t == "directory") fileTypes.push_back('d');
                    else if (t == "l" || t == "symlink") fileTypes.push_back('l');
                    else if (t == "x" || t == "executable") fileTypes.push_back('x');
                }
            } else if (arg == "-x" || arg == "--exec") {
                if (i + 1 < argc) {
                    execCommand = argv[++i];
                }
            } else if (arg == "-c" || arg == "--color") {
                if (i + 1 < argc) {
                    std::string val = argv[++i];
                    if (val == "never") {
                        colorOutput = false;
                        colorOutputExplicit = true;
                    } else if (val == "always") {
                        colorOutput = true;
                        colorOutputExplicit = true;
                    }
                }
            } else if (arg[0] == '-' && arg.length() > 1) {
                std::cerr << Color::RED << "Error: Unknown option '" << arg << "'" << Color::RESET << "\n";
                std::cerr << "For usage information, run: fd --help\n";
                return false;
            } else {
                searchPaths.push_back(arg);
            }
        }

        if (!searchPaths.empty()) {
            if (searchPaths.size() == 1 && fs::exists(searchPaths[0]) && fs::is_directory(searchPaths[0])) {
                // Single directory path
            } else {
                pattern = searchPaths.front();
                searchPaths.erase(searchPaths.begin());
            }
        }

        if (searchPaths.empty()) {
            searchPaths.push_back(".");
        }

        if (!colorOutputExplicit && !ConsoleEnvironment::ShouldUseColorByDefault()) {
            colorOutput = false;
        }

        return true;
    }

    void PrintHelp() const {
        std::cout << R"(fd(1)                   CrossShell for UNIX Reference Manual                    fd(1)

    NAME
        fd - simple, fast and user-friendly alternative to find

    SYNOPSIS
        fd [OPTIONS] [PATTERN] [PATH...]

    DESCRIPTION
        fd is a fast, intuitive search tool for finding filesystem entries matching
        regular expressions or globs, respecting gitignore and hidden files by default.

    OPTIONS
        -H, --hidden
            Search hidden files and directories.

        -I, --no-ignore
            Do not respect .(git)ignore files.

        -s, --case-sensitive
            Case-sensitive search (default is smart case).

        -t, --type TYPE
            Filter by entry type: f (file), d (directory), l (symlink), e (empty).

        -e, --extension EXT
            Filter by file extension.

        -x, --exec COMMAND
            Execute a command for each search result.

        --json, --csv, --table
            Format search results as JSON, CSV, or table.

        --pipe COMMAND
            Pipe matches directly to COMMAND.

        -h, --help
            Display this reference manual.

        -V, --version
            Output version information and exit.

    EXAMPLES
        fd "\.cpp$" src/
            Find all C++ source files in src/.

        fd -e pdf --json
            Find all PDF files and emit JSON list.

    CrossShell for UNIX                                                     fd(1)
)";
    }

    void PrintVersion() const {
        std::cout << Color::BOLD << Color::CYAN << "fd" << Color::RESET << " version 1.1.0\n";
    }
};

// ============================================================================
// 5. SEARCH ENGINE & APPLICATION CONTROLLER
// ============================================================================

class FdSearchEngine {
public:
    static void SearchDirectory(const std::string& rootPath, const FdOptions& config, const CompiledPattern& compiledPattern, std::vector<std::string>& matches) {
        fs::path base(rootPath);
        if (!fs::exists(base)) {
            std::cerr << Color::RED << "Error: Search path '" << rootPath << "' does not exist." << Color::RESET << "\n";
            return;
        }

        auto options = fs::directory_options::skip_permission_denied;
        
        try {
            fs::recursive_directory_iterator it(base, options), end;
            while (it != end) {
                const auto& entry = *it;
                int currentDepth = it.depth();
                std::string filename = PathHelper::PathToUtf8(entry.path().filename());
                bool skipEntry = false;

                if (config.maxDepth != -1 && entry.is_directory() && currentDepth >= config.maxDepth) {
                    it.disable_recursion_pending();
                }
                if (config.maxDepth != -1 && currentDepth > config.maxDepth) {
                    ++it;
                    continue;
                }

                if (!skipEntry && entry.is_directory() && PathHelper::IsIgnoredDirectory(filename, config.includeIgnored)) {
                    it.disable_recursion_pending();
                    skipEntry = true;
                }

                if (!skipEntry && !config.includeHidden && PathHelper::IsHidden(entry)) {
                    if (entry.is_directory()) {
                        it.disable_recursion_pending();
                    }
                    skipEntry = true;
                }

                if (!skipEntry && config.minDepth != -1 && currentDepth < config.minDepth) {
                    skipEntry = true;
                }

                if (!skipEntry && !config.fileTypes.empty()) {
                    bool typeMatch = false;
                    for (char t : config.fileTypes) {
                        if (t == 'f' && entry.is_regular_file()) typeMatch = true;
                        if (t == 'd' && entry.is_directory()) typeMatch = true;
                        if (t == 'l' && entry.is_symlink()) typeMatch = true;
                        if (t == 'x' && PathHelper::IsExecutable(entry)) typeMatch = true;
                    }
                    if (!typeMatch) {
                        skipEntry = true;
                    }
                }

                if (!skipEntry && !config.extensions.empty()) {
                    std::string ext = entry.path().extension().string();
                    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
                    ext = PathHelper::ToLower(ext);

                    bool extMatch = false;
                    for (const auto& targetExt : config.extensions) {
                        std::string normalizedTarget = PathHelper::ToLower(targetExt);
                        if (!normalizedTarget.empty() && normalizedTarget[0] == '.') {
                            normalizedTarget = normalizedTarget.substr(1);
                        }
                        if (normalizedTarget == ext) {
                            extMatch = true;
                            break;
                        }
                    }
                    if (!extMatch) {
                        skipEntry = true;
                    }
                }

                if (!skipEntry && CompiledPatternMatcher::Matches(filename, compiledPattern)) {
                    fs::path outPath = config.absolutePaths ? fs::absolute(entry.path()) : entry.path();
                    std::string displayStr = PathHelper::PathToUtf8(outPath);

                    if (!config.execCommand.empty()) {
                        std::string cmd = CommandExecutor::BuildExecCommand(config.execCommand, outPath);
                        if (!CommandExecutor::ExecuteCommandDirect(cmd)) {
                            std::cerr << Color::RED << "Error: failed to execute command: " << cmd << Color::RESET << "\n";
                        }
                    } else {
                        if (config.outputFormat != OutputFormat::Default || !config.pipeCommand.empty()) {
                            matches.push_back(displayStr);
                        } else if (config.print0) {
                            std::cout << displayStr << '\0';
                        } else {
                            std::cout << PathHelper::FormatPath(entry, displayStr, config.colorOutput) << "\n";
                        }
                    }
                }

                ++it;
            }
        } catch (const std::exception& e) {
            std::cerr << Color::RED << "Error during traversal: " << e.what() << Color::RESET << "\n";
        }
    }
};

class FdApplication {
public:
    int Run(int argc, char* argv[]) const {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(nullptr);

        ConsoleEnvironment::InitConsole();
        FdOptions config;

        if (!config.Parse(argc, argv)) {
            return 1;
        }

        if (config.showHelp) {
            config.PrintHelp();
            return 0;
        }
        if (config.showVersion) {
            config.PrintVersion();
            return 0;
        }

        CompiledPattern compiledPattern = CompiledPatternMatcher::PreparePattern(config.pattern, config.caseMode, config.fixedStrings);

        std::vector<std::string> matches;
        for (const auto& path : config.searchPaths) {
            FdSearchEngine::SearchDirectory(path, config, compiledPattern, matches);
        }

        if (config.outputFormat != OutputFormat::Default || !config.pipeCommand.empty()) {
            std::ostringstream output;
            if (config.outputFormat == OutputFormat::Json) {
                output << "[";
                for (size_t i = 0; i < matches.size(); ++i) {
                    if (i) output << ",";
                    output << "{\"path\":\"" << matches[i] << "\"}";
                }
                output << "]\n";
            } else if (config.outputFormat == OutputFormat::Csv) {
                output << "path\n";
                for (const auto& match : matches) output << match << "\n";
            } else {
                output << "PATH\n----\n";
                for (const auto& match : matches) output << match << "\n";
            }

            std::string text = output.str();
            if (!config.pipeCommand.empty()) {
                FILE* pipe = _popen(config.pipeCommand.c_str(), "w");
                if (pipe) {
                    fwrite(text.data(), 1, text.size(), pipe);
                    _pclose(pipe);
                }
            } else {
                std::cout << text;
            }
        }

        return 0;
    }
};

int main(int argc, char* argv[]) {
    FdApplication app;
    return app.Run(argc, argv);
}