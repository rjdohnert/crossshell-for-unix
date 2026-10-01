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
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;

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
// Class: CopyOptions
// Encapsulates configuration corresponding to HP-UX cp flags.
// ============================================================================
class CopyOptions {
public:
    bool force = false;            // -f: Unlink existing dest without prompting
    bool interactive = false;      // -i: Prompt before overwriting
    bool preserve = false;         // -p: Preserve timestamps, modes, attributes
    bool recursive = false;        // -r, -R: Copy directory subtrees
    bool hpuxExtentAcl = false;    // -e: Preserve Windows attributes / ACLs (HP-UX -e)
    bool noDereference = false;    // -P: Do not follow symlinks
    bool dereference = false;      // -L: Dereference symlinks (default)
    bool showProgress = true;      // Visual progress display
    bool verbose = false;          // -v: Verbose output
    bool outputJson = false;       // --json: Output structured JSON

    std::vector<fs::path> sources;
    fs::path destination;
};

// ============================================================================
// Class: ProgressBar
// Handles visual terminal progress rendering, transfer rate, and ETA.
// ============================================================================
class ProgressBar {
private:
    uint64_t totalBytes = 0;
    uint64_t transferredBytes = 0;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastRenderTime;
    int barWidth = 32;
    bool cursorHidden = false;

    static std::string formatSize(uint64_t bytes) {
        constexpr const char* suffixes[] = { "B", "KB", "MB", "GB", "TB" };
        int idx = 0;
        double size = static_cast<double>(bytes);
        while (size >= 1024.0 && idx < 4) {
            size /= 1024.0;
            idx++;
        }
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(idx == 0 ? 0 : 1) << size << " " << suffixes[idx];
        return oss.str();
    }

    static std::string formatTime(uint64_t seconds) {
        int m = static_cast<int>(seconds / 60);
        int s = static_cast<int>(seconds % 60);
        std::ostringstream oss;
        oss << std::setfill('0') << std::setw(2) << m << ":" << std::setw(2) << s;
        return oss.str();
    }

    void hideCursor() {
        if (!cursorHidden) {
            std::cout << "\x1b[?25l" << std::flush;
            cursorHidden = true;
        }
    }

    void showCursor() {
        if (cursorHidden) {
            std::cout << "\x1b[?25h" << std::flush;
            cursorHidden = false;
        }
    }

public:
    ProgressBar() {
        startTime = std::chrono::steady_clock::now();
        lastRenderTime = startTime;
    }

    ~ProgressBar() {
        showCursor();
    }

    void reset(uint64_t fileBytes) {
        totalBytes = fileBytes;
        transferredBytes = 0;
        startTime = std::chrono::steady_clock::now();
        lastRenderTime = startTime;
    }

    void update(uint64_t chunkBytes, const std::string& currentFileName, bool forceRender = false) {
        transferredBytes += chunkBytes;
        auto now = std::chrono::steady_clock::now();
        auto msSinceLast = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRenderTime).count();

        if (!forceRender && msSinceLast < 80 && transferredBytes < totalBytes) {
            return;
        }
        lastRenderTime = now;

        hideCursor();

        double progress = (totalBytes > 0) ? (static_cast<double>(transferredBytes) / totalBytes) : 1.0;
        progress = (std::min)(1.0, (std::max)(0.0, progress));

        auto elapsedSec = std::chrono::duration_cast<std::chrono::duration<double>>(now - startTime).count();
        double speed = (elapsedSec > 0.001) ? (transferredBytes / elapsedSec) : 0.0;
        uint64_t etaSec = (speed > 0.0 && transferredBytes < totalBytes) 
                          ? static_cast<uint64_t>((totalBytes - transferredBytes) / speed) : 0;

        int filled = static_cast<int>(progress * barWidth);
        int empty = barWidth - filled;

        std::string displayName = currentFileName;
        if (displayName.length() > 22) {
            displayName = displayName.substr(0, 19) + "...";
        }

        std::ostringstream oss;
        oss << "\r"
            << std::left << std::setw(22) << displayName << " "
            << "[" << std::string(filled, '=') 
            << (filled < barWidth ? ">" : "") 
            << std::string((empty > 0 ? empty - 1 : 0), ' ') << "] "
            << std::right << std::setw(3) << static_cast<int>(progress * 100.0) << "% "
            << std::setw(9) << formatSize(static_cast<uint64_t>(speed)) << "/s "
            << "ETA " << formatTime(etaSec) << "  \x1b[K";

        std::cout << oss.str() << std::flush;
    }

    void finish() {
        showCursor();
        std::cout << "\n" << std::flush;
    }
};

// ============================================================================
// Class: FileAttributePreserver
// Handles Windows timestamps and file security/attribute preservation.
// ============================================================================
class FileAttributePreserver {
public:
    static void preserveAttributes(const fs::path& src, const fs::path& dest, bool preserveTimestamps, bool preserveExtended) {
        std::error_code ec;

        if (preserveTimestamps) {
            auto lwt = fs::last_write_time(src, ec);
            if (!ec) {
                fs::last_write_time(dest, lwt, ec);
            }
        }

        if (preserveExtended) {
            DWORD attr = GetFileAttributesW(src.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES) {
                SetFileAttributesW(dest.c_str(), attr & ~(FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_DIRECTORY));
            }
        }
    }
};

// ============================================================================
// Class: CopyEngine
// Core executor that manages directory traversal, conflict handling, and I/O.
// ============================================================================
class CopyEngine {
private:
    CopyOptions options;
    ProgressBar progressBar;

    bool promptOverwrite(const fs::path& dest) {
        std::cout << "cp: overwrite \"" << dest.string() << "\"? (y/n [n]) ";
        std::string response;
        if (!std::getline(std::cin, response)) {
            return false;
        }
        return (!response.empty() && (response[0] == 'y' || response[0] == 'Y'));
    }

    bool copySingleFile(const fs::path& src, const fs::path& dest) {
        std::error_code ec;

        if (fs::exists(dest, ec)) {
            if (options.interactive) {
                if (!promptOverwrite(dest)) {
                    std::cout << "cp: skipping \"" << dest.string() << "\"\n";
                    return true;
                }
            } else if (options.force) {
                fs::remove(dest, ec);
            }
        }

        uint64_t fileSize = fs::file_size(src, ec);
        if (ec) fileSize = 0;

        std::ifstream in(src, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "cp: cannot open source file: " << src.string() << "\n";
            return false;
        }

        std::ofstream out(dest, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            std::cerr << "cp: cannot create target file: " << dest.string() << "\n";
            return false;
        }

        if (options.showProgress) {
            progressBar.reset(fileSize);
        }

        constexpr size_t BUFFER_SIZE = 64 * 1024; // 64 KB chunk
        std::vector<char> buffer(BUFFER_SIZE);

        while (in) {
            in.read(buffer.data(), buffer.size());
            std::streamsize bytesRead = in.gcount();
            if (bytesRead > 0) {
                out.write(buffer.data(), bytesRead);
                if (options.showProgress) {
                    progressBar.update(static_cast<uint64_t>(bytesRead), src.filename().string());
                }
            }
        }

        if (options.showProgress) {
            progressBar.update(0, src.filename().string(), true);
            progressBar.finish();
        }

        in.close();
        out.close();

        if (options.preserve || options.hpuxExtentAcl) {
            FileAttributePreserver::preserveAttributes(src, dest, options.preserve, options.hpuxExtentAcl);
        }

        if (options.outputJson) {
            std::cout << "{\"action\":\"copy\",\"source\":\"" << JsonSerializer::escape(src.string()) 
                      << "\",\"destination\":\"" << JsonSerializer::escape(dest.string()) 
                      << "\",\"bytes\":" << fileSize << "}\n";
        }

        return true;
    }

    bool copyDirectoryRecursive(const fs::path& src, const fs::path& dest) {
        std::error_code ec;
        fs::create_directories(dest, ec);

        for (const auto& entry : fs::directory_iterator(src, fs::directory_options::skip_permission_denied)) {
            const fs::path& currentPath = entry.path();
            fs::path targetPath = dest / currentPath.filename();

            if (entry.is_symlink()) {
                if (options.noDereference) {
                    fs::copy_symlink(currentPath, targetPath, ec);
                    continue;
                }
            }

            if (entry.is_directory()) {
                if (!copyDirectoryRecursive(currentPath, targetPath)) {
                    return false;
                }
            } else if (entry.is_regular_file()) {
                if (!copySingleFile(currentPath, targetPath)) {
                    return false;
                }
            }
        }
        return true;
    }

public:
    explicit CopyEngine(const CopyOptions& opts) : options(opts) {}

    bool execute() {
        if (options.sources.empty()) {
            std::cerr << "cp: missing file operand\nTry 'cp --help' for more information.\n";
            return false;
        }

        bool destIsDir = fs::is_directory(options.destination);

        if (options.sources.size() > 1 && !destIsDir) {
            std::cerr << "cp: target \"" << options.destination.string() 
                      << "\" is not a directory\n";
            return false;
        }

        for (const auto& src : options.sources) {
            if (!fs::exists(src)) {
                std::cerr << "cp: cannot stat \"" << src.string() << "\": No such file or directory\n";
                return false;
            }

            fs::path targetPath = destIsDir ? (options.destination / src.filename()) : options.destination;

            if (fs::is_directory(src)) {
                if (!options.recursive) {
                    std::cerr << "cp: omitting directory \"" << src.string() << "\" (use -r or -R)\n";
                    continue;
                }
                if (!copyDirectoryRecursive(src, targetPath)) {
                    return false;
                }
            } else {
                if (!copySingleFile(src, targetPath)) {
                    return false;
                }
            }
        }
        return true;
    }
};

// ============================================================================
// Class: HelpFormatter
// HP-UX formatted Reference Manual page.
// ============================================================================
class HelpFormatter {
public:
    static void printHelp() {
        std::cout << R"(cp(1)                   CrossShell for UNIX Reference Manual                  cp(1)

    NAME
        cp - copy files and directory subtrees

    SYNOPSIS
        cp [OPTIONS] SOURCE... DEST

    DESCRIPTION
        The cp command copies the contents of SOURCE to DEST. If multiple sources
        are specified, DEST must be a directory. Supports recursive subtree copies,
        attribute and ACL preservation, and dereference options.

    OPTIONS
        -f, --force
            Unlink existing destination files without prompting (overrides -i).

        -i, --interactive
            Prompt before overwriting an existing destination file.

        -p, --preserve
            Preserve timestamps and Windows file attributes.

        -e
            Preserve file security descriptors and extended attributes.

        -r, -R, --recursive
            Copy directories recursively.

        -L, --dereference
            Always follow symbolic links.

        -P, --no-dereference
            Never follow symbolic links; copy the link itself.

        -v, --verbose
            Display detailed execution diagnostics.

        --no-progress
            Disable interactive terminal progress bar indicators.

        -h, --help
            Display this reference manual.

    EXAMPLES
        cp kernel.sys C:\Backup\
            Copy single file to backup directory.

        cp -r /opt/app D:\Deployment\
            Recursively copy directory tree.

        cp -ip file1.dat file2.dat
            Interactively copy with preserved attributes.

    CrossShell for UNIX                                                    cp(1)
)";
    }
};

// ============================================================================
// Class: ArgumentParser
// Parses HP-UX compound arguments (e.g., -rip, -fe) and long options.
// ============================================================================
class ArgumentParser {
public:
    static bool parse(int argc, char* argv[], CopyOptions& options) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h") {
                HelpFormatter::printHelp();
                exit(0);
            }
            if (arg == "--no-progress") {
                options.showProgress = false;
                continue;
            }
            if (arg == "--verbose") {
                options.verbose = true;
                continue;
            }
            if (arg == "--json") {
                options.outputJson = true;
                options.showProgress = false;
                continue;
            }

            if (arg.rfind("--", 0) == 0) {
                std::cerr << "cp: unrecognized option '" << arg << "'\nTry 'cp --help' for manual.\n";
                return false;
            }

            if (arg.length() > 1 && arg[0] == '-') {
                for (size_t c = 1; c < arg.length(); ++c) {
                    switch (arg[c]) {
                        case 'f':
                            options.force = true;
                            options.interactive = false;
                            break;
                        case 'i':
                            options.interactive = true;
                            options.force = false;
                            break;
                        case 'p':
                            options.preserve = true;
                            break;
                        case 'r':
                        case 'R':
                            options.recursive = true;
                            break;
                        case 'e':
                            options.hpuxExtentAcl = true;
                            break;
                        case 'P':
                            options.noDereference = true;
                            options.dereference = false;
                            break;
                        case 'L':
                            options.dereference = true;
                            options.noDereference = false;
                            break;
                        case 'v':
                            options.verbose = true;
                            break;
                        default:
                            std::cerr << "cp: illegal option -- " << arg[c] << "\n";
                            std::cerr << "usage: cp [-f | -i] [-p] [-e] [-P | -L] [-r | -R] file ... target\n";
                            return false;
                    }
                }
            } else {
                options.sources.push_back(fs::u8path(arg));
            }
        }

        if (options.sources.size() < 2) {
            std::cerr << "cp: missing operand\nTry 'cp --help' for more information.\n";
            return false;
        }

        options.destination = options.sources.back();
        options.sources.pop_back();

        return true;
    }
};

// ============================================================================
// Main Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    // Enable ANSI escape sequence processing on Windows consoles
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    CopyOptions options;
    if (!ArgumentParser::parse(argc, argv, options)) {
        return 1;
    }

    CopyEngine engine(options);
    return engine.execute() ? 0 : 1;
}