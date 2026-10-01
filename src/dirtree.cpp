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
#if defined(__INTELLISENSE__) && !defined(_HAS_CXX17)
#  define _HAS_CXX17 1
#endif
#if __has_include(<filesystem>)
#  include <filesystem>
namespace fs = std::filesystem;
#else
#  include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <cstdint>
#include <io.h>
#include <cstdio>
#include <memory>

// ============================================================================
// 1. ANSI COLORS & DATA MODELS
// ============================================================================

namespace Color {
    const std::string RESET     = "\033[0m";
    const std::string BOLD      = "\033[1m";
    const std::string BLUE      = "\033[1;34m";
    const std::string CYAN      = "\033[1;36m";
    const std::string GREEN     = "\033[1;32m";
    const std::string YELLOW    = "\033[1;33m";
    const std::string RED       = "\033[1;31m";
    const std::string MAGENTA   = "\033[1;35m";
    const std::string GRAY      = "\033[90m";
    const std::string B_CYAN    = "\033[96m";
}

struct TreeStats {
    size_t dirCount = 0;
    size_t fileCount = 0;
};

enum class OutputFormat {
    Default = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

// ============================================================================
// 2. CONSOLE ENVIRONMENT & PATH HELPERS
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
    static std::string WideToUtf8(const std::wstring& ws) {
        if (ws.empty()) return std::string();
        int count = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (count <= 0) return std::string();

        std::vector<char> out(static_cast<size_t>(count));
        WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), count, nullptr, nullptr);
        return std::string(out.data());
    }

    static std::string PathToUtf8(const fs::path& p) {
        return WideToUtf8(p.wstring());
    }

    static std::string FormatSize(std::uintmax_t size) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        double s = static_cast<double>(size);
        int idx = 0;
        while (s >= 1024.0 && idx < 4) {
            s /= 1024.0;
            idx++;
        }
        std::ostringstream oss;
        if (idx == 0) {
            oss << size << " B";
        } else {
            oss << std::fixed << std::setprecision(1) << s << " " << units[idx];
        }
        return oss.str();
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

    static std::string GetFileColor(const fs::directory_entry& entry) {
        if (entry.is_directory()) return Color::CYAN;
        if (entry.is_symlink()) return Color::MAGENTA;

        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".ps1" || ext == ".com") {
            return Color::GREEN;
        }
        if (ext == ".zip" || ext == ".tar" || ext == ".gz" || ext == ".7z" || ext == ".rar" || ext == ".iso") {
            return Color::RED;
        }
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || ext == ".py" || ext == ".js" || ext == ".cs" || ext == ".rs" || ext == ".go") {
            return Color::YELLOW;
        }
        return Color::RESET;
    }
};

// ============================================================================
// 3. OPTIONS & REPORTER
// ============================================================================

class TreeOptions {
public:
    std::string rootPath = ".";
    bool showAll = false;
    bool dirsOnly = false;
    bool showSizes = false;
    bool useAscii = false;
    bool useColor = true;
    bool colorModeExplicit = false;
    int maxLevel = -1;
    std::vector<std::string> extensions;
    OutputFormat outputFormat = OutputFormat::Default;
    std::string pipeCommand;
    bool showHelp = false;
    bool showVersion = false;

    bool Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--json") { outputFormat = OutputFormat::Json; continue; }
            if (arg == "--csv") { outputFormat = OutputFormat::Csv; continue; }
            if (arg == "--table") { outputFormat = OutputFormat::Table; continue; }
            if (arg == "--pipe" && i + 1 < argc) { pipeCommand = argv[++i]; continue; }

            if (arg == "-h" || arg == "--help") {
                showHelp = true;
                return true;
            } else if (arg == "-V" || arg == "--version") {
                showVersion = true;
                return true;
            } else if (arg == "-a" || arg == "--all") {
                showAll = true;
            } else if (arg == "-d" || arg == "--dirs-only") {
                dirsOnly = true;
            } else if (arg == "-s" || arg == "--sizes") {
                showSizes = true;
            } else if (arg == "--ascii") {
                useAscii = true;
            } else if (arg == "-L" || arg == "--level") {
                if (i + 1 < argc) maxLevel = std::atoi(argv[++i]);
            } else if (arg.rfind("--level=", 0) == 0) {
                maxLevel = std::atoi(arg.substr(8).c_str());
            } else if (arg == "-e" || arg == "--extension") {
                if (i + 1 < argc) extensions.push_back(argv[++i]);
            } else if (arg == "-c" || arg == "--color") {
                if (i + 1 < argc) {
                    std::string val = argv[++i];
                    if (val == "never") {
                        useColor = false;
                        colorModeExplicit = true;
                    } else if (val == "always") {
                        useColor = true;
                        colorModeExplicit = true;
                    } else if (val == "auto") {
                        useColor = ConsoleEnvironment::ShouldUseColorByDefault();
                        colorModeExplicit = true;
                    }
                }
            } else if (arg[0] == '-' && arg.length() > 1) {
                std::cerr << Color::RED << "Error: Unknown option '" << arg << "'" << Color::RESET << "\n";
                std::cerr << "For usage information, run: dirtree --help\n";
                return false;
            } else {
                rootPath = arg;
            }
        }
        return true;
    }

    void PrintHelp() const {
           std::cout << R"(dirtree(1)               CrossShell for UNIX Reference Manual                  dirtree(1)

    NAME
        dirtree - Display a tree directory hierarchy

    SYNOPSIS
        dirtree [OPTIONS] [PATH]

    DESCRIPTION
        Displays a directory tree with optional colors, sizes, extension filters,
        structured output, and recursion limits.

    OPTIONS
        -a, --all             Include hidden files and system folders.
        -d, --dirs-only       List directories only.
        -L, --level LEVEL     Set maximum recursion depth.
        -s, --sizes           Display human-readable file sizes.
        -e, --extension EXT   Filter by extension; repeatable.
        --ascii               Use ASCII branch characters.
        -c, --color WHEN      always, never, or auto.
        --json, --csv, --table Select output format.
        --pipe COMMAND        Send output through COMMAND.
        -h, --help            Display this comprehensive reference manual.
        -V, --version         Display version information and exit.

    EXAMPLES
        dirtree
        dirtree -L 2 C:\Projects\App
        dirtree -a -d
        dirtree -e cpp -e hpp -s

    EXIT STATUS
        0          Help, version, or successful traversal.
        1          Invalid options or traversal/output failure.

    CrossShell for UNIX                                                       dirtree(1)
    )";
           return;

        std::cout << Color::BOLD << Color::B_CYAN << "dirtree" << Color::RESET 
                  << " 2.0.0\n"
                  << "A modern directory tree visualization tool with ANSI color formatting.\n\n"
                  << Color::BOLD << "USAGE:" << Color::RESET << "\n"
                  << "    dirtree [OPTIONS] [PATH]\n\n"
                  << Color::BOLD << "ARGUMENTS:" << Color::RESET << "\n"
                  << "    " << Color::GREEN << "[PATH]" << Color::RESET 
                  << "                  Target root directory to map [default: current directory].\n\n"
                  << Color::BOLD << "OPTIONS:" << Color::RESET << "\n"
                  << "    " << Color::YELLOW << "-a, --all" << Color::RESET 
                  << "               Include hidden files and system folders.\n"
                  << "    " << Color::YELLOW << "-d, --dirs-only" << Color::RESET 
                  << "         List directories only.\n"
                  << "    " << Color::YELLOW << "-L, --level <LEVEL>" << Color::RESET 
                  << "     Set the maximum directory recursion depth.\n"
                  << "    " << Color::YELLOW << "-s, --sizes" << Color::RESET 
                  << "             Print human-readable file sizes alongside filenames.\n"
                  << "    " << Color::YELLOW << "-e, --extension <EXT>" << Color::RESET 
                  << "   Filter files by extension (can be specified multiple times).\n"
                  << "    " << Color::YELLOW << "    --ascii" << Color::RESET 
                  << "             Use standard ASCII branch characters instead of Unicode box-drawing.\n"
                  << "    " << Color::YELLOW << "-c, --color <WHEN>" << Color::RESET 
                  << "      Control color output [always, never, auto].\n"
                  << "    " << Color::YELLOW << "-h, --help" << Color::RESET 
                  << "              Print this comprehensive help message.\n"
                  << "    " << Color::YELLOW << "-V, --version" << Color::RESET 
                  << "           Print version information.\n"
                  << "    --json, --csv, --table  Select structured output.\n"
                  << "    --pipe COMMAND          Send output through COMMAND.\n\n"
                  << Color::BOLD << "EXAMPLES:" << Color::RESET << "\n"
                  << "    Visualize current working directory:\n"
                  << "        " << Color::CYAN << "dirtree" << Color::RESET << "\n\n"
                  << "    Visualize target path up to 2 recursion levels:\n"
                  << "        " << Color::CYAN << "dirtree -L 2 C:\\Projects\\App" << Color::RESET << "\n\n"
                  << "    Display directories only, including hidden folders:\n"
                  << "        " << Color::CYAN << "dirtree -a -d" << Color::RESET << "\n\n"
                  << "    Filter tree to show C++ source files with file sizes:\n"
                  << "        " << Color::CYAN << "dirtree -e cpp -e hpp -s" << Color::RESET << "\n";
    }

    void PrintVersion() const {
        std::cout << Color::BOLD << Color::B_CYAN << "dirtree" << Color::RESET << " version 2.0.0\n";
    }
};

// ============================================================================
// 4. TREE TRAVERSAL ENGINE
// ============================================================================

class TreeTraversalEngine {
public:
    static void PrintTree(const fs::path& currentPath, const std::string& prefix, int currentLevel, const TreeOptions& config, TreeStats& stats, std::ostream& out) {
        if (config.maxLevel != -1 && currentLevel >= config.maxLevel) {
            return;
        }

        std::vector<fs::directory_entry> entries;
        try {
            auto opts = fs::directory_options::skip_permission_denied;
            for (const auto& entry : fs::directory_iterator(currentPath, opts)) {
                if (!config.showAll && PathHelper::IsHidden(entry)) {
                    continue;
                }

                if (config.dirsOnly && !entry.is_directory()) {
                    continue;
                }

                if (!config.extensions.empty() && entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
                    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

                    bool match = false;
                    for (const auto& targetExt : config.extensions) {
                        std::string t = targetExt;
                        if (!t.empty() && t[0] == '.') t = t.substr(1);
                        std::transform(t.begin(), t.end(), t.begin(), ::tolower);
                        if (t == ext) { match = true; break; }
                    }
                    if (!match) continue;
                }

                entries.push_back(entry);
            }
        } catch (const std::exception&) {
            return;
        }

        std::sort(entries.begin(), entries.end(), [](const fs::directory_entry& a, const fs::directory_entry& b) {
            if (a.is_directory() != b.is_directory()) {
                return a.is_directory() > b.is_directory();
            }
            std::string nameA = a.path().filename().string();
            std::string nameB = b.path().filename().string();
            std::transform(nameA.begin(), nameA.end(), nameA.begin(), ::tolower);
            std::transform(nameB.begin(), nameB.end(), nameB.begin(), ::tolower);
            return nameA < nameB;
        });

        size_t count = entries.size();
        for (size_t i = 0; i < count; ++i) {
            const auto& entry = entries[i];
            bool isLast = (i == count - 1);

            std::string branch = config.useAscii ? (isLast ? "`-- " : "|-- ") : (isLast ? "└── " : "├── ");
            std::string name = PathHelper::PathToUtf8(entry.path().filename());

            if (entry.is_directory()) {
                stats.dirCount++;
            } else {
                stats.fileCount++;
            }

            if (config.useColor) {
                out << Color::GRAY << prefix << branch << Color::RESET;
            } else {
                out << prefix << branch;
            }

            std::string color = config.useColor ? PathHelper::GetFileColor(entry) : "";
            out << color << name;
            if (config.useColor) {
                out << Color::RESET;
            }

            if (config.showSizes && entry.is_regular_file()) {
                try {
                    std::uintmax_t sz = entry.file_size();
                    if (config.useColor) {
                        out << Color::GRAY << " (" << PathHelper::FormatSize(sz) << ")" << Color::RESET;
                    } else {
                        out << " (" << PathHelper::FormatSize(sz) << ")";
                    }
                } catch (...) {}
            }

            out << "\n";

            if (entry.is_directory()) {
                std::string childPrefix = prefix + (config.useAscii ? (isLast ? "    " : "|   ") : (isLast ? "    " : "│   "));
                PrintTree(entry.path(), childPrefix, currentLevel + 1, config, stats, out);
            }
        }
    }
};

// ============================================================================
// 5. APPLICATION CONTROLLER
// ============================================================================

class DirtreeApplication {
public:
    int Run(int argc, char* argv[]) const {
        std::ios_base::sync_with_stdio(false);
        std::cin.tie(nullptr);

        ConsoleEnvironment::InitConsole();
        TreeOptions config;

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

        fs::path root(config.rootPath);
        if (!fs::exists(root)) {
            std::cerr << Color::RED << "Error: Target path '" << config.rootPath << "' does not exist." << Color::RESET << "\n";
            return 1;
        }

        if (!config.colorModeExplicit) {
            config.useColor = ConsoleEnvironment::ShouldUseColorByDefault();
        }

        std::ostringstream captured;
        std::ostream* outStream = (config.outputFormat != OutputFormat::Default || !config.pipeCommand.empty()) ? &captured : &std::cout;

        *outStream << (config.useColor ? Color::B_CYAN : "") << PathHelper::PathToUtf8(root) << (config.useColor ? Color::RESET : "") << "\n";

        TreeStats stats;
        TreeTraversalEngine::PrintTree(root, "", 0, config, stats, *outStream);

        *outStream << "\n";
        if (config.useColor) {
            *outStream << Color::BOLD << stats.dirCount << Color::RESET;
        } else {
            *outStream << stats.dirCount;
        }
        *outStream << " directories";
        if (!config.dirsOnly) {
            *outStream << ", ";
            if (config.useColor) {
                *outStream << Color::BOLD << stats.fileCount << Color::RESET;
            } else {
                *outStream << stats.fileCount;
            }
            *outStream << " files";
        }
        *outStream << "\n";

        if (config.outputFormat != OutputFormat::Default || !config.pipeCommand.empty()) {
            std::string data = captured.str();
            std::string text = (config.outputFormat == OutputFormat::Json) ? "{\"root\":\"" + PathHelper::PathToUtf8(root) + "\",\"output\":\"" + data + "\"}\n" :
                               (config.outputFormat == OutputFormat::Csv) ? "root,output\n" + PathHelper::PathToUtf8(root) + ",\"" + data + "\"\n" :
                               "ROOT\tOUTPUT\n" + PathHelper::PathToUtf8(root) + "\t" + data;

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
    DirtreeApplication app;
    return app.Run(argc, argv);
}