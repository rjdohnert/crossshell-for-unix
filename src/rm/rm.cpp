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
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL EXEMPLARY, OR
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
#include <string_view>
#include <memory>
#include <filesystem>
#include <algorithm>
#include <cstdint>
#include <sstream>

namespace fs = std::filesystem;

// ============================================================================
// 1. ENUMS & CONFIGURATION
// ============================================================================

enum class InteractiveMode {
    Never,
    Once,
    Always
};

struct RmOptions {
    bool force{false};
    bool recursive{false};
    bool removeEmptyDirs{false};
    bool verbose{false};
    bool dryRun{false};
    bool preserveRoot{true};
    bool nullDelimited{false};
    bool readStdin{false};
    bool showHelp{false};
    bool showVersion{false};
    InteractiveMode interactive{InteractiveMode::Never};

    std::vector<std::string> targets;
};

// ============================================================================
// 2. CONSOLE PROMPTER
// ============================================================================

class ConsolePrompter {
public:
    static bool Prompt(const std::string& message) {
        std::cerr << message << " (y/n)? ";
        std::cerr.flush();

        // If STDIN is redirected, read prompt response from physical console
        if (!_isatty(_fileno(stdin))) {
            HANDLE hConIn = CreateFileW(L"CONIN$", GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            if (hConIn != INVALID_HANDLE_VALUE) {
                char ch = 0;
                DWORD bytesRead = 0;
                ReadFile(hConIn, &ch, 1, &bytesRead, nullptr);
                CloseHandle(hConIn);
                std::cerr << "\n";
                return (ch == 'y' || ch == 'Y');
            }
        }

        std::string response;
        if (std::cin >> response) {
            return (!response.empty() && (response[0] == 'y' || response[0] == 'Y'));
        }
        return false;
    }
};

// ============================================================================
// 3. LOW-LEVEL WINDOWS FILE OPERATIONS
// ============================================================================

class WindowsFileOperations {
public:
    static bool IsRootPath(const fs::path& p) {
        fs::path abs = fs::absolute(p);
        return (abs == abs.root_path());
    }

    static void StripReadOnly(const fs::path& path) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
            SetFileAttributesW(path.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
        }
    }

    static bool IsWriteProtected(const fs::path& path) {
        DWORD attrs = GetFileAttributesW(path.c_str());
        return (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY));
    }
};

// ============================================================================
// 4. PIPELINE STREAM INGESTION HANDLER
// ============================================================================

class PipeStreamHandler {
public:
    static std::vector<std::string> ReadTargetsFromStream(bool nullDelimited) {
        std::vector<std::string> paths;
        _setmode(_fileno(stdin), _O_BINARY);

        if (nullDelimited) {
            std::string current;
            char ch;
            while (std::cin.get(ch)) {
                if (ch == '\0') {
                    if (!current.empty()) {
                        paths.push_back(current);
                        current.clear();
                    }
                } else {
                    current.push_back(ch);
                }
            }
            if (!current.empty()) paths.push_back(current);
        } else {
            std::string line;
            while (std::getline(std::cin, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (!line.empty()) {
                    paths.push_back(line);
                }
            }
        }
        return paths;
    }
};

// ============================================================================
// 5. OPTION PARSER & HELP SYSTEM
// ============================================================================

class OptionParser {
public:
    static void ShowHelp() {
        std::cout << R"(rm(1)                      CrossShell for UNIX Reference Manual                     rm(1)

    NAME
        rm - remove files or directories

    SYNOPSIS
        rm [OPTIONS] FILE...

    DESCRIPTION
        rm removes each specified FILE or directory from the filesystem. By
        default, it does not remove directories unless -r, -R, or -d is specified.
        If '-' or '--from-stdin' is provided (or when standard input is
        redirected without positional arguments), paths are read from standard
        input.

    OPTIONS
        -f, --force
            Ignore nonexistent files and arguments, never prompt. Automatically
            clears Windows Read-Only file attributes when deleting.

        -i
            Prompt for confirmation before every removal.

        -I
            Prompt once before removing more than three files or when removing
            recursively.

        --interactive[=when]
            Prompt according to <when>: 'never', 'once' (-I), or 'always' (-i).

        -r, -R, --recursive
            Remove directories and their contents recursively.

        -d, --dir
            Remove empty directories.

        -v, --verbose
            Display diagnostic messages detailing each removed file or directory.

        -n, --dry-run
            Simulate execution without modifying the filesystem.

        --preserve-root
            Do not remove drive roots (e.g., C:\ or /) [default].

        --no-preserve-root
            Do not treat drive roots specially.

        -0, --null
            Read input paths separated by NUL (0 byte) instead of whitespace.

        --from-stdin
            Read target paths from standard input.

        -h, --help
            Display this reference manual and exit.

        --version
            Display version information and exit.

    EXAMPLES
        rm file1.txt file2.log
            Remove file1.txt and file2.log.

        rm -rf build C:\Temp\cache
            Force recursively delete build directory and cache.

        rm -rvn folder_to_delete
            Simulate recursive removal with verbose output.

        rm -d empty_dir
            Remove empty directory empty_dir.

    CrossShell for UNIX                                                          rm(1)
)";
    }

    static void ShowVersion() {
        std::cout << "rm v1.2.0)\n";
        std::cout << "Copyright (C) 2026, Roberto J Dohnert.\n";
    }

    bool Parse(int argc, char* argv[], RmOptions& opts) const {
        std::vector<std::string> positional;

        for (int i = 1; i < argc; ++i) {
            std::string_view arg = argv[i];

            if (arg == "-h" || arg == "--help") {
                opts.showHelp = true;
                return true;
            } else if (arg == "--version") {
                opts.showVersion = true;
                return true;
            } else if (arg == "-f" || arg == "--force") {
                opts.force = true;
                opts.interactive = InteractiveMode::Never;
            } else if (arg == "-i") {
                opts.interactive = InteractiveMode::Always;
                opts.force = false;
            } else if (arg == "-I") {
                opts.interactive = InteractiveMode::Once;
            } else if (arg.rfind("--interactive", 0) == 0) {
                size_t eqPos = arg.find('=');
                if (eqPos == std::string_view::npos) {
                    opts.interactive = InteractiveMode::Always;
                } else {
                    std::string_view mode = arg.substr(eqPos + 1);
                    if (mode == "always") opts.interactive = InteractiveMode::Always;
                    else if (mode == "once") opts.interactive = InteractiveMode::Once;
                    else if (mode == "never") opts.interactive = InteractiveMode::Never;
                }
            } else if (arg == "-r" || arg == "-R" || arg == "--recursive") {
                opts.recursive = true;
            } else if (arg == "-d" || arg == "--dir") {
                opts.removeEmptyDirs = true;
            } else if (arg == "-v" || arg == "--verbose") {
                opts.verbose = true;
            } else if (arg == "-n" || arg == "--dry-run") {
                opts.dryRun = true;
            } else if (arg == "-0" || arg == "--null") {
                opts.nullDelimited = true;
            } else if (arg == "--from-stdin") {
                opts.readStdin = true;
            } else if (arg == "--preserve-root") {
                opts.preserveRoot = true;
            } else if (arg == "--no-preserve-root") {
                opts.preserveRoot = false;
            } else if (arg == "-") {
                opts.readStdin = true;
            } else if (arg.length() > 1 && arg[0] == '-' && arg[1] != '-') {
                for (size_t j = 1; j < arg.length(); ++j) {
                    switch (arg[j]) {
                        case 'f': opts.force = true; opts.interactive = InteractiveMode::Never; break;
                        case 'i': opts.interactive = InteractiveMode::Always; opts.force = false; break;
                        case 'I': opts.interactive = InteractiveMode::Once; break;
                        case 'r':
                        case 'R': opts.recursive = true; break;
                        case 'd': opts.removeEmptyDirs = true; break;
                        case 'v': opts.verbose = true; break;
                        case 'n': opts.dryRun = true; break;
                        case '0': opts.nullDelimited = true; break;
                        default:
                            std::cerr << "rm: invalid option -- '" << arg[j] << "'\n";
                            std::cerr << "Try 'rm --help' for more information.\n";
                            return false;
                    }
                }
            } else {
                positional.push_back(std::string(arg));
            }
        }

        if (positional.empty() && !opts.readStdin) {
            if (!_isatty(_fileno(stdin))) {
                opts.readStdin = true;
            }
        }

        opts.targets = std::move(positional);
        return true;
    }
};

// ============================================================================
// 6. CORE REMOVAL ENGINE
// ============================================================================

class RmEngine {
private:
    RmOptions m_options;
    uint64_t m_removedCount{0};

public:
    explicit RmEngine(RmOptions opts) : m_options(std::move(opts)) {}

    int Run() {
        if (m_options.readStdin) {
            auto pipedPaths = PipeStreamHandler::ReadTargetsFromStream(m_options.nullDelimited);
            m_options.targets.insert(m_options.targets.end(), pipedPaths.begin(), pipedPaths.end());
        }

        if (m_options.targets.empty()) {
            if (!m_options.force) {
                std::cerr << "rm: missing operand\n";
                std::cerr << "Try 'rm --help' for more information.\n";
                return 1;
            }
            return 0;
        }

        if (m_options.interactive == InteractiveMode::Once) {
            if (m_options.targets.size() > 3 || m_options.recursive) {
                std::ostringstream msg;
                msg << "rm: remove " << m_options.targets.size() << " arguments";
                if (m_options.recursive) msg << " recursively";
                if (!ConsolePrompter::Prompt(msg.str())) {
                    return 0;
                }
            }
        }

        int exitCode = 0;
        for (const auto& target : m_options.targets) {
            if (!ProcessTarget(target)) {
                exitCode = 1;
            }
        }

        return exitCode;
    }

private:
    bool ProcessTarget(const fs::path& target) {
        std::error_code ec;
        fs::file_status status = fs::symlink_status(target, ec);

        if (!fs::exists(status)) {
            if (!m_options.force) {
                std::cerr << "rm: cannot remove '" << target.string() << "': No such file or directory\n";
                return false;
            }
            return true;
        }

        if (m_options.preserveRoot && WindowsFileOperations::IsRootPath(target)) {
            std::cerr << "rm: it is dangerous to operate recursively on '" << target.string() << "'\n";
            std::cerr << "rm: use --no-preserve-root to override this failsafe\n";
            return false;
        }

        if (fs::is_directory(status)) {
            return ProcessDirectory(target);
        } else {
            return ProcessFile(target);
        }
    }

    bool ProcessFile(const fs::path& file) {
        if (m_options.interactive == InteractiveMode::Always) {
            std::string promptMsg = "rm: remove regular file '" + file.string() + "'";
            if (!ConsolePrompter::Prompt(promptMsg)) {
                return true;
            }
        } else if (!m_options.force && WindowsFileOperations::IsWriteProtected(file)) {
            std::string promptMsg = "rm: remove write-protected regular file '" + file.string() + "'";
            if (!ConsolePrompter::Prompt(promptMsg)) {
                return true;
            }
        }

        if (m_options.verbose) {
            std::cout << "removed '" << file.string() << "'\n";
        }

        if (m_options.dryRun) return true;

        if (m_options.force) {
            WindowsFileOperations::StripReadOnly(file);
        }

        std::error_code ec;
        if (!fs::remove(file, ec)) {
            if (!m_options.force) {
                std::cerr << "rm: cannot remove '" << file.string() << "': " << ec.message() << "\n";
                return false;
            }
        }

        m_removedCount++;
        return true;
    }

    bool ProcessDirectory(const fs::path& dir) {
        if (!m_options.recursive && !m_options.removeEmptyDirs) {
            std::cerr << "rm: cannot remove '" << dir.string() << "': Is a directory\n";
            return false;
        }

        if (m_options.removeEmptyDirs && !m_options.recursive) {
            if (m_options.interactive == InteractiveMode::Always) {
                if (!ConsolePrompter::Prompt("rm: remove directory '" + dir.string() + "'")) {
                    return true;
                }
            }

            if (m_options.verbose) {
                std::cout << "removed directory '" << dir.string() << "'\n";
            }

            if (m_options.dryRun) return true;

            std::error_code ec;
            if (!fs::remove(dir, ec)) {
                std::cerr << "rm: failed to remove '" << dir.string() << "': Directory not empty\n";
                return false;
            }
            return true;
        }

        if (m_options.interactive == InteractiveMode::Always) {
            if (!ConsolePrompter::Prompt("rm: descend into directory '" + dir.string() + "'")) {
                return true;
            }
        }

        return RemoveDirectoryRecursive(dir);
    }

    bool RemoveDirectoryRecursive(const fs::path& dir) {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(dir, ec)) {
            const auto& path = entry.path();
            if (entry.is_directory()) {
                RemoveDirectoryRecursive(path);
            } else {
                ProcessFile(path);
            }
        }

        if (m_options.interactive == InteractiveMode::Always) {
            if (!ConsolePrompter::Prompt("rm: remove directory '" + dir.string() + "'")) {
                return true;
            }
        }

        if (m_options.verbose) {
            std::cout << "removed directory '" << dir.string() << "'\n";
        }

        if (m_options.dryRun) return true;

        if (m_options.force) {
            WindowsFileOperations::StripReadOnly(dir);
        }

        fs::remove(dir, ec);
        return true;
    }
};

// ============================================================================
// 7. MAIN APPLICATION CONTROLLER
// ============================================================================

class RmApplication {
private:
    OptionParser m_parser;

public:
    int Run(int argc, char* argv[]) {
        SetConsoleOutputCP(CP_UTF8);

        RmOptions options;
        if (!m_parser.Parse(argc, argv, options)) {
            return 1;
        }

        if (options.showHelp) {
            OptionParser::ShowHelp();
            return 0;
        }

        if (options.showVersion) {
            OptionParser::ShowVersion();
            return 0;
        }

        RmEngine engine(options);
        return engine.Run();
    }
};

int main(int argc, char* argv[]) {
    try {
        RmApplication app;
        return app.Run(argc, argv);
    } catch (const std::exception& ex) {
        std::cerr << "rm: fatal error: " << ex.what() << "\n";
        return 1;
    }
}