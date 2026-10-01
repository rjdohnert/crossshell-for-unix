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
// Class: MoveOptions
// Encapsulates configuration corresponding to HP-UX mv flags.
// ============================================================================
class MoveOptions {
public:
    bool force = false;            // -f: Overwrite without prompting, ignore permissions
    bool interactive = false;      // -i: Prompt before overwriting destination
    bool hpuxExtentAcl = false;    // -e: Preserve Windows attributes / ACLs across volumes
    bool showProgress = true;      // Dynamic visual progress display
    bool verbose = false;          // -v: Verbose move reporting
    bool outputJson = false;       // --json: Output structured JSON

    std::vector<fs::path> sources;
    fs::path destination;
};

// ============================================================================
// Class: ProgressBar
// Terminal UI widget for transfer progress, byte counts, rate, and ETA.
// ============================================================================
class ProgressBar {
private:
    uint64_t totalBytes = 0;
    uint64_t transferredBytes = 0;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastRenderTime;
    int barWidth = 30;
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

        if (!forceRender && msSinceLast < 75 && transferredBytes < totalBytes) {
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
        if (displayName.length() > 20) {
            displayName = displayName.substr(0, 17) + "...";
        }

        std::ostringstream oss;
        oss << "\r"
            << std::left << std::setw(20) << displayName << " "
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
// Manages Windows timestamps and extended security attributes.
// ============================================================================
class FileAttributePreserver {
public:
    static void preserve(const fs::path& src, const fs::path& dest, bool preserveExtended) {
        std::error_code ec;
        auto lwt = fs::last_write_time(src, ec);
        if (!ec) {
            fs::last_write_time(dest, lwt, ec);
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
// Class: MoveEngine
// Executes atomic renames or cross-volume chunked moves with progress tracking.
// ============================================================================
class MoveEngine {
private:
    MoveOptions options;
    ProgressBar progressBar;

    bool promptOverwrite(const fs::path& dest) {
        std::cout << "mv: overwrite \"" << dest.string() << "\"? (y/n [n]) ";
        std::string response;
        if (!std::getline(std::cin, response)) {
            return false;
        }
        return (!response.empty() && (response[0] == 'y' || response[0] == 'Y'));
    }

    bool prepareDestination(const fs::path& dest, bool& replaceTarget) {
        std::error_code ec;
        replaceTarget = options.force || !options.interactive;

        if (fs::exists(dest, ec)) {
            if (options.interactive) {
                if (!promptOverwrite(dest)) {
                    return false;
                }
                replaceTarget = true;
            }
            if (replaceTarget) {
                // Clear read-only attribute if forced or user approved overwrite
                DWORD attr = GetFileAttributesW(dest.c_str());
                if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY)) {
                    SetFileAttributesW(dest.c_str(), attr & ~FILE_ATTRIBUTE_READONLY);
                }
            }
        }
        return true;
    }

    bool copyAndUnlinkFile(const fs::path& src, const fs::path& dest) {
        std::error_code ec;
        uint64_t fileSize = fs::file_size(src, ec);
        if (ec) fileSize = 0;

        std::ifstream in(src, std::ios::binary);
        if (!in.is_open()) {
            std::cerr << "mv: cannot open \"" << src.string() << "\": Permission denied\n";
            return false;
        }

        std::ofstream out(dest, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            std::cerr << "mv: cannot create target \"" << dest.string() << "\"\n";
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

        FileAttributePreserver::preserve(src, dest, options.hpuxExtentAcl);

        // Remove source once successfully copied
        fs::remove(src, ec);
        if (ec) {
            std::cerr << "mv: cannot unlink source \"" << src.string() << "\": " << ec.message() << "\n";
            return false;
        }

        if (options.outputJson) {
            std::cout << "{\"action\":\"move\",\"source\":\"" << JsonSerializer::escape(src.string())
                      << "\",\"destination\":\"" << JsonSerializer::escape(dest.string()) << "\"}\n";
        }

        return true;
    }

    bool moveDirectoryCrossVolume(const fs::path& src, const fs::path& dest) {
        std::error_code ec;
        fs::create_directories(dest, ec);

        for (const auto& entry : fs::directory_iterator(src, fs::directory_options::skip_permission_denied)) {
            const fs::path& currentPath = entry.path();
            fs::path targetPath = dest / currentPath.filename();

            if (entry.is_directory()) {
                if (!moveDirectoryCrossVolume(currentPath, targetPath)) {
                    return false;
                }
            } else {
                if (!copyAndUnlinkFile(currentPath, targetPath)) {
                    return false;
                }
            }
        }

        fs::remove(src, ec);
        return true;
    }

    bool moveItem(const fs::path& src, const fs::path& dest) {
        bool replaceTarget = false;
        if (!prepareDestination(dest, replaceTarget)) {
            std::cout << "mv: skipping \"" << src.string() << "\"\n";
            return true;
        }

        DWORD flags = MOVEFILE_WRITE_THROUGH;
        if (replaceTarget) {
            flags |= MOVEFILE_REPLACE_EXISTING;
        }

        // Try atomic rename first
        if (MoveFileExW(src.c_str(), dest.c_str(), flags)) {
            if (options.verbose) {
                std::cout << "renamed: " << src.string() << " -> " << dest.string() << "\n";
            }
            if (options.outputJson) {
                std::cout << "{\"action\":\"move\",\"source\":\"" << JsonSerializer::escape(src.string())
                          << "\",\"destination\":\"" << JsonSerializer::escape(dest.string()) << "\"}\n";
            }
            return true;
        }

        DWORD error = GetLastError();

        // If error is ERROR_NOT_SAME_DEVICE, perform cross-volume fallback
        if (error == ERROR_NOT_SAME_DEVICE) {
            if (fs::is_directory(src)) {
                return moveDirectoryCrossVolume(src, dest);
            } else {
                return copyAndUnlinkFile(src, dest);
            }
        }

        std::cerr << "mv: cannot move \"" << src.string() << "\" to \"" 
                  << dest.string() << "\": System Error " << error << "\n";
        return false;
    }

public:
    explicit MoveEngine(const MoveOptions& opts) : options(opts) {}

    bool execute() {
        if (options.sources.empty()) {
            std::cerr << "mv: missing file operand\nTry 'mv --help' for more information.\n";
            return false;
        }

        bool destIsDir = fs::is_directory(options.destination);

        if (options.sources.size() > 1 && !destIsDir) {
            std::cerr << "mv: target \"" << options.destination.string() 
                      << "\" is not a directory\n";
            return false;
        }

        for (const auto& src : options.sources) {
            if (!fs::exists(src)) {
                std::cerr << "mv: cannot stat \"" << src.string() << "\": No such file or directory\n";
                return false;
            }

            fs::path targetPath = destIsDir ? (options.destination / src.filename()) : options.destination;

            if (!moveItem(src, targetPath)) {
                return false;
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
        std::cout << R"(mv(1)                   CrossShell for UNIX Reference Manual                      mv(1)

    NAME
        mv - move or rename files and directory hierarchies

    SYNOPSIS
        mv [OPTIONS] SOURCE... DESTINATION

    DESCRIPTION
        The mv command moves or renames files and directories. If the
        destination is an existing directory, the source files are moved
        into that directory.

        If source and destination reside on the same filesystem volume,
        mv executes an instantaneous atomic rename. When moving across
        different volumes or file system boundaries (e.g., C: to D:), mv
        falls back to a chunked stream transfer with dynamic progress
        reporting, reproduces all timestamps and security descriptors, and
        unlinks the source.

    OPTIONS
        -e
            Preserve file attributes, Windows Security Descriptors, and
            Extended Attributes during cross-filesystem moves.

        -f, --force
            Overwrite target files without prompting, even if permissions
            would otherwise prevent writing. Overrides prior -i options.

        -i, --interactive
            Prompt for confirmation before overwriting an existing target
            file. Overrides prior -f options.

        -v, --verbose
            Display detailed operational notices upon file moves.

        --json
            Output structured JSON summary of operations.

        --no-progress
            Disable the interactive progress bar during cross-device
            transfers.

        -h, --help
            Display this reference manual.

    EXAMPLES
        mv update.tar update_old.tar
            Rename a file in place.

        mv file1.log file2.log C:\Archive\
            Move multiple files into a target directory.

        mv -i database.mdf D:\LiveMount\
            Prompt before overwriting destination files across drives.

        mv -fe SourceTree E:\BackupTree\
            Move a directory tree preserving extended attributes and forcing overwrite.

    CrossShell for UNIX                                                    mv(1)
)";
    }
};

// ============================================================================
// Class: ArgumentParser
// Parses HP-UX compound parameters (e.g., -fi, -fe) and standard options.
// ============================================================================
class ArgumentParser {
public:
    static bool parse(int argc, char* argv[], MoveOptions& options) {
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
            if (arg == "--verbose" || arg == "-v") {
                options.verbose = true;
                continue;
            }
            if (arg == "--json") {
                options.outputJson = true;
                options.showProgress = false;
                continue;
            }

            if (arg.rfind("--", 0) == 0) {
                std::cerr << "mv: unrecognized option '" << arg << "'\nTry 'mv --help' for manual.\n";
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
                        case 'e':
                            options.hpuxExtentAcl = true;
                            break;
                        case 'v':
                            options.verbose = true;
                            break;
                        default:
                            std::cerr << "mv: illegal option -- " << arg[c] << "\n";
                            std::cerr << "usage: mv [-f | -i] [-e] file ... target\n";
                            return false;
                    }
                }
            } else {
                options.sources.push_back(fs::u8path(arg));
            }
        }

        if (options.sources.size() < 2) {
            std::cerr << "mv: missing destination file operand after '" 
                      << (!options.sources.empty() ? options.sources[0].string() : "") << "'\n"
                      << "Try 'mv --help' for more information.\n";
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

    MoveOptions options;
    if (!ArgumentParser::parse(argc, argv, options)) {
        return 1;
    }

    MoveEngine engine(options);
    return engine.execute() ? 0 : 1;
}