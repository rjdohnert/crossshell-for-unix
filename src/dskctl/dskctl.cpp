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

#include <algorithm>
#include <cctype>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <memory>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Advapi32.lib")

#undef min
#undef max

namespace fs {
using namespace std::filesystem;
}

// ANSI Terminal Formatting
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define BOLD    "\033[1m"

// ============================================================================
// 1. RAII GUARDS & DATA MODELS
// ============================================================================

class ConsoleEnvironment {
public:
    static void EnableVirtualTerminal() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
};

class ScopedProcessToken {
public:
    explicit ScopedProcessToken(HANDLE token = nullptr) : m_token(token) {}
    ~ScopedProcessToken() { Close(); }

    ScopedProcessToken(const ScopedProcessToken&) = delete;
    ScopedProcessToken& operator=(const ScopedProcessToken&) = delete;

    HANDLE Get() const { return m_token; }
    bool IsValid() const { return m_token != nullptr && m_token != INVALID_HANDLE_VALUE; }

    void Close() {
        if (IsValid()) {
            CloseHandle(m_token);
            m_token = nullptr;
        }
    }

private:
    HANDLE m_token;
};

struct CleanTarget {
    std::string name;
    std::string flag;
    std::string description;
    std::vector<std::wstring> rawPaths;
    bool selected = false;
    bool destructive = false;
    std::string risk = "low";
};

enum class OutputFormat {
    Default = 0,
    Json = 1,
    Csv = 2,
    Table = 3
};

struct DskctlConfig {
    bool dryRun = true;
    bool verbose = false;
    bool force = false;
    bool trash = false;
    bool execute = false;
    OutputFormat outputFormat = OutputFormat::Default;
    std::string pipeCommand;
};

struct CleanSummary {
    uint64_t totalBytesFreed = 0;
    size_t totalFilesRemoved = 0;
    size_t totalSkipped = 0;
    size_t totalErrors = 0;
};

// ============================================================================
// 2. STRING & PATH HELPERS
// ============================================================================

class StringHelper {
public:
    static std::string ToLowerCopy(const std::string& value) {
        std::string out = value;
        std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return out;
    }

    static std::string TrimCopy(const std::string& value) {
        const char* ws = " \t\r\n";
        size_t start = value.find_first_not_of(ws);
        if (start == std::string::npos) {
            return std::string();
        }
        size_t end = value.find_last_not_of(ws);
        return value.substr(start, end - start + 1);
    }

    static std::string WideToUtf8(const std::wstring& value) {
        if (value.empty()) {
            return std::string();
        }

        int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (required <= 0) {
            return std::string();
        }

        std::string output(static_cast<size_t>(required), '\0');
        int written = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, &output[0], required, nullptr, nullptr);
        if (written <= 0) {
            return std::string();
        }
        output.resize(static_cast<size_t>(written - 1));
        return output;
    }

    static fs::path ExpandEnvPath(const std::wstring& rawPath) {
        wchar_t buffer[MAX_PATH];
        DWORD size = ExpandEnvironmentStringsW(rawPath.c_str(), buffer, MAX_PATH);
        if (size == 0 || size > MAX_PATH) {
            return fs::path();
        }
        return fs::path(buffer);
    }

    static std::string FormatBytes(uint64_t bytes) {
        const char* units[] = { "B", "KB", "MB", "GB", "TB" };
        int i = 0;
        double count = static_cast<double>(bytes);
        while (count >= 1024 && i < 4) {
            count /= 1024;
            i++;
        }
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2) << count << " " << units[i];
        return stream.str();
    }
};

// ============================================================================
// 3. SECURITY & LOGGING INSPECTORS
// ============================================================================

class SecurityInspector {
public:
    static bool IsCurrentUserAdmin() {
        BOOL isAdmin = FALSE;
        HANDLE hToken = NULL;
        if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            ScopedProcessToken scopedToken(hToken);
            TOKEN_ELEVATION elevation;
            DWORD cbSize = sizeof(TOKEN_ELEVATION);
            if (GetTokenInformation(scopedToken.Get(), TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
                isAdmin = elevation.TokenIsElevated;
            }
        }
        return isAdmin != 0;
    }

    static std::vector<fs::path> BuildAllowedRoots() {
        std::vector<fs::path> roots;
        const std::vector<std::wstring> rawRoots = {
            L"%TEMP%",
            L"%WINDIR%\\Temp",
            L"%WINDIR%\\SoftwareDistribution\\Download",
            L"%WINDIR%\\Logs",
            L"%WINDIR%\\Panther",
            L"%WINDIR%\\MEMORY.DMP",
            L"%WINDIR%\\Minidump",
            L"%LOCALAPPDATA%\\CrashDumps",
            L"%PROGRAMDATA%\\Microsoft\\Windows\\WER",
            L"%LOCALAPPDATA%\\D3DSCache",
            L"%LOCALAPPDATA%\\NVIDIA\\DXCache",
            L"%LOCALAPPDATA%\\AMD\\DxCache",
            L"%PROGRAMDATA%\\Microsoft\\Windows\\DeliveryOptimization\\Cache",
            L"%LOCALAPPDATA%\\Microsoft\\Windows\\INetCache"
        };

        for (const auto& raw : rawRoots) {
            fs::path root = StringHelper::ExpandEnvPath(raw);
            if (!root.empty()) {
                roots.push_back(root.lexically_normal());
            }
        }
        return roots;
    }

    static bool IsAllowedPath(const fs::path& path) {
        if (path.empty()) {
            return false;
        }

        std::error_code ec;
        fs::path resolved = fs::weakly_canonical(path, ec);
        if (ec) {
            resolved = path;
        }

        std::wstring normalized = resolved.wstring();
        std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](wchar_t c) {
            return std::towlower(c);
        });

        auto isWithinRoot = [&](const fs::path& root) {
            std::wstring rootString = root.wstring();
            std::transform(rootString.begin(), rootString.end(), rootString.begin(), [](wchar_t c) {
                return std::towlower(c);
            });
            return normalized == rootString || (normalized.rfind(rootString, 0) == 0 && normalized.size() > rootString.size() && normalized[rootString.size()] == L'\\');
        };

        for (const auto& root : BuildAllowedRoots()) {
            if (isWithinRoot(root)) {
                return true;
            }
        }
        return false;
    }
};

class DskctlLogger {
public:
    static fs::path GetLogPath() {
        wchar_t buffer[MAX_PATH] = {};
        DWORD size = GetTempPathW(MAX_PATH, buffer);
        if (size == 0 || size >= MAX_PATH) {
            return fs::temp_directory_path() / "dskctl.log";
        }
        return fs::path(buffer) / "dskctl.log";
    }

    static void AppendLogLine(const std::string& line) {
        std::ofstream log(GetLogPath(), std::ios::app);
        if (log.is_open()) {
            log << line << "\n";
        }
    }
};

class DskctlConfigManager {
public:
    static fs::path GetConfigPath() {
        wchar_t buffer[MAX_PATH] = {};
        DWORD size = GetTempPathW(MAX_PATH, buffer);
        if (size == 0 || size >= MAX_PATH) {
            return fs::temp_directory_path() / "dskctl.conf";
        }
        return fs::path(buffer) / "dskctl.conf";
    }

    static bool LoadConfig(const fs::path& path, DskctlConfig& config) {
        std::ifstream input(path);
        if (!input.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(input, line)) {
            const size_t separator = line.find('=');
            if (separator == std::string::npos) {
                continue;
            }

            std::string key = StringHelper::ToLowerCopy(StringHelper::TrimCopy(line.substr(0, separator)));
            std::string value = StringHelper::TrimCopy(line.substr(separator + 1));
            if (key == "preview_only") {
                config.dryRun = (value == "1" || StringHelper::ToLowerCopy(value) == "true" || StringHelper::ToLowerCopy(value) == "yes");
                if (config.dryRun) {
                    config.execute = false;
                }
            } else if (key == "trash") {
                config.trash = (value == "1" || StringHelper::ToLowerCopy(value) == "true" || StringHelper::ToLowerCopy(value) == "yes");
            } else if (key == "force") {
                config.force = (value == "1" || StringHelper::ToLowerCopy(value) == "true" || StringHelper::ToLowerCopy(value) == "yes");
            } else if (key == "verbose") {
                config.verbose = (value == "1" || StringHelper::ToLowerCopy(value) == "true" || StringHelper::ToLowerCopy(value) == "yes");
            }
        }

        return true;
    }
};

// ============================================================================
// 4. CLEANING & TRAVERSAL ENGINE
// ============================================================================

class DiskCleanerEngine {
public:
    static bool MoveToRecycleBin(const fs::path& path) {
        std::wstring source = path.native();
        source.push_back(L'\0');
        source.push_back(L'\0');

        SHFILEOPSTRUCTW op = {};
        op.hwnd = nullptr;
        op.wFunc = FO_DELETE;
        op.pFrom = source.c_str();
        op.pTo = nullptr;
        op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;
        int result = SHFileOperationW(&op);
        return result == 0;
    }

    static void CollectPreviewItems(const fs::path& path, std::vector<std::string>& items, size_t& count) {
        std::error_code ec;
        if (!fs::exists(path, ec)) {
            return;
        }

        if (!fs::is_directory(path, ec)) {
            items.push_back(path.string());
            ++count;
            return;
        }

        for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied, ec)) {
            try {
                items.push_back(entry.path().string());
                ++count;

                std::error_code statusEc;
                if (fs::is_directory(entry.status(statusEc))) {
                    try {
                        CollectPreviewItems(entry.path(), items, count);
                    } catch (...) {
                    }
                }
            } catch (...) {
            }
        }
    }

    static bool CleanDirectory(const fs::path& path, const DskctlConfig& config, uint64_t& totalBytesFreed, size_t& filesRemoved, size_t& skippedEntries, size_t& errorCount) {
        std::error_code ec;
        if (!fs::exists(path, ec)) {
            skippedEntries++;
            return true;
        }

        if (!SecurityInspector::IsAllowedPath(path)) {
            std::cerr << RED << "[SKIP] Refusing to operate on non-allowlisted path: " << path.string() << RESET << "\n";
            skippedEntries++;
            return false;
        }

        if (!fs::is_directory(path, ec)) {
            uint64_t size = fs::file_size(path, ec);
            if (ec) size = 0;

            if (config.verbose) {
                std::cout << "  " << (config.dryRun ? "[WOULD UNLINK] " : (config.trash ? "[TRASH] " : "[UNLINKING] ")) << path.string() << "\n";
            }

            if (config.dryRun) {
                totalBytesFreed += size;
                filesRemoved++;
                return true;
            }

            bool success = true;
            if (config.trash) {
                success = MoveToRecycleBin(path);
            } else {
                std::error_code delEc;
                success = fs::remove(path, delEc);
                if (!delEc && success) {
                    totalBytesFreed += size;
                    filesRemoved++;
                }
            }

            if (!success) {
                errorCount++;
                skippedEntries++;
                std::cerr << RED << "[SKIP] Locked or inaccessible: " << path.string() << RESET << "\n";
            }
            return success;
        }

        for (const auto& entry : fs::directory_iterator(path, fs::directory_options::skip_permission_denied, ec)) {
            try {
                uint64_t entrySize = 0;
                if (fs::is_regular_file(entry.status())) {
                    entrySize = entry.file_size(ec);
                }

                if (config.verbose) {
                    std::cout << "  " << (config.dryRun ? "[WOULD UNLINK] " : (config.trash ? "[TRASH] " : "[UNLINKING] ")) << entry.path().string() << "\n";
                }

                if (!config.dryRun) {
                    std::error_code delEc;
                    bool success = true;
                    if (config.trash) {
                        success = MoveToRecycleBin(entry.path());
                    } else {
                        success = static_cast<bool>(fs::remove_all(entry.path(), delEc));
                    }
                    if (!delEc && success) {
                        totalBytesFreed += entrySize;
                        filesRemoved++;
                    } else {
                        errorCount++;
                        skippedEntries++;
                        std::cerr << RED << "[SKIP] Locked or inaccessible: " << entry.path().string() << RESET << "\n";
                    }
                } else {
                    totalBytesFreed += entrySize;
                    filesRemoved++;
                }
            } catch (...) {
                skippedEntries++;
                std::cerr << RED << "[SKIP] Could not process entry due to an exception: " << path.string() << RESET << "\n";
            }
        }

        return true;
    }

    static void CleanRecycleBin(const DskctlConfig& config, uint64_t& totalBytesFreed) {
        SHQUERYRBINFO rbInfo = { sizeof(SHQUERYRBINFO) };
        if (SUCCEEDED(SHQueryRecycleBinW(NULL, &rbInfo))) {
            totalBytesFreed += rbInfo.i64Size;
            if (!config.dryRun) {
                SHEmptyRecycleBinW(NULL, NULL, SHERB_NOCONFIRMATION | SHERB_NOPROGRESSUI | SHERB_NOSOUND);
            }
        }
    }
};

// ============================================================================
// 5. OPTIONS & REPORTER
// ============================================================================

class DskctlOptions {
public:
    DskctlConfig config;
    std::vector<CleanTarget> targets = {
        {"User Temp Files", "--temp", "Cleans local user temp directory (%TEMP%)", {L"%TEMP%"}, false, false, "low"},
        {"System Temp Files", "--system-temp", "Cleans Windows system temp directory", {L"%WINDIR%\\Temp"}, false, false, "medium"},
        {"Windows Update Cache", "--update-cache", "Cleans Windows Update download packages", {L"%WINDIR%\\SoftwareDistribution\\Download"}, false, false, "medium"},
        {"Memory Crash Dumps", "--error-dumps", "Cleans MEMORY.DMP and Minidump files", {L"%WINDIR%\\MEMORY.DMP", L"%WINDIR%\\Minidump"}, false, true, "high"},
        {"System Logs", "--logs", "Cleans Windows Setup & System Log files", {L"%WINDIR%\\Logs", L"%WINDIR%\\Panther"}, false, true, "high"},
        {"Recycle Bin", "--recycle-bin", "Empties system-wide Recycle Bin", {}, false, true, "high"},
        {"Windows Error Reports", "--wer", "Cleans WER crash dumps and report queues", {L"%LOCALAPPDATA%\\CrashDumps", L"%PROGRAMDATA%\\Microsoft\\Windows\\WER"}, false, true, "high"},
        {"DirectX Shader Cache", "--shaders", "Cleans GPU/DirectX shader caches", {L"%LOCALAPPDATA%\\D3DSCache", L"%LOCALAPPDATA%\\NVIDIA\\DXCache", L"%LOCALAPPDATA%\\AMD\\DxCache"}, false, true, "medium"},
        {"Delivery Optimization", "--delivery-opt", "Cleans Delivery Optimization files", {L"%PROGRAMDATA%\\Microsoft\\Windows\\DeliveryOptimization\\Cache"}, false, true, "medium"},
        {"Internet Cache", "--inet-cache", "Cleans INetCache temporary files", {L"%LOCALAPPDATA%\\Microsoft\\Windows\\INetCache"}, false, true, "medium"}
    };

    bool showHelp = false;

    bool Parse(int argc, char* argv[]) {
        DskctlConfigManager::LoadConfig(DskctlConfigManager::GetConfigPath(), config);

        bool targetSpecified = false;
        bool safeOnly = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--json") { config.outputFormat = OutputFormat::Json; continue; }
            if (arg == "--csv") { config.outputFormat = OutputFormat::Csv; continue; }
            if (arg == "--table") { config.outputFormat = OutputFormat::Table; continue; }
            if (arg == "--pipe" && i + 1 < argc) { config.pipeCommand = argv[++i]; continue; }

            if (arg == "-h" || arg == "--help" || arg == "help") {
                showHelp = true;
                return true;
            } else if (arg == "-n" || arg == "--dry-run") {
                config.dryRun = true;
                config.execute = false;
            } else if (arg == "-x" || arg == "--execute") {
                config.execute = true;
                config.dryRun = false;
            } else if (arg == "-t" || arg == "--trash") {
                config.trash = true;
            } else if (arg == "-v" || arg == "--verbose") {
                config.verbose = true;
            } else if (arg == "-f" || arg == "--force") {
                config.force = true;
            } else if (arg == "-a" || arg == "--all") {
                for (auto& t : targets) t.selected = true;
                targetSpecified = true;
            } else if (arg == "-s" || arg == "--safe") {
                safeOnly = true;
                targetSpecified = true;
            } else {
                bool matched = false;
                for (auto& t : targets) {
                    if (arg == t.flag) {
                        t.selected = true;
                        targetSpecified = matched = true;
                        break;
                    }
                }
                if (!matched) {
                    std::cerr << RED << "[EINVAL] Invalid flag or target: " << arg << RESET << "\n";
                    std::cerr << "Run '" << argv[0] << " --help' for available flags.\n";
                    return false;
                }
            }
        }

        if (safeOnly) {
            for (auto& t : targets) {
                if (t.risk == "low" || t.risk == "medium") {
                    t.selected = true;
                }
            }
        } else if (!targetSpecified) {
            for (auto& t : targets) {
                if (t.risk == "low") {
                    t.selected = true;
                }
            }
        }

        return true;
    }
};

class DskctlReporter {
public:
    static void PrintHelp(const char* exeName, const std::vector<CleanTarget>& targets) {
        std::cout << R"(dskctl(1)                 CrossShell for UNIX Reference Manual                 dskctl(1)

    NAME
        dskctl - preview-first disk cleanup and cache maintenance utility

    SYNOPSIS
        dskctl [OPTIONS] [TARGET_FLAGS...]

    DESCRIPTION
        Provides a preview-first Windows cleanup utility for temporary and
        cache-related files. It can inspect and optionally remove data from
        known temporary locations, Windows update caches, crash dumps, log
        directories, and the Recycle Bin.

        Provides standard CrossShell semantics and integrates natively with
        Windows console pipelines, standard streams, and file paths.

    OPTIONS
        -a, --all
            Select all available disk cleanup targets.

        -s, --safe
            Select only safe, non-destructive cleanup targets.

        -n, --dry-run
            Preview cleanup actions without removing files (default).

        -x, --execute
            Perform the cleanup actions after previewing.

        -t, --trash
            Move items to the Recycle Bin instead of deleting permanently.

        -v, --verbose
            Output full paths of all evaluated files.

        -f, --force
            Bypass interactive confirmation prompt.

        --json
            Output summary formatted as JSON.

        --csv
            Output summary formatted as CSV.

        --table
            Output summary formatted as a table.

        --pipe COMMAND
            Send formatted summary through COMMAND pipeline.

        -h, --help
            Display this reference manual and exit.

    TARGETS
        --temp
            Cleans local user temp directory (%TEMP%).

        --system-temp
            Cleans Windows system temp directory (%WINDIR%\Temp).

        --update-cache
            Cleans Windows Update download packages.

        --error-dumps
            Cleans MEMORY.DMP and Minidump crash files.

        --logs
            Cleans Windows Setup and system log files.

        --recycle-bin
            Empties system-wide Recycle Bin.

        --wer
            Cleans WER crash dumps and report queues.

        --shaders
            Cleans GPU and DirectX shader caches.

        --delivery-opt
            Cleans Delivery Optimization cache files.

        --inet-cache
            Cleans INetCache temporary files.

    EXAMPLES
        dskctl -n
            Preview standard safe cleanup targets without deleting files.

        dskctl --temp --recycle-bin -x
            Clean temporary files and empty the Recycle Bin.

        dskctl -a -x -f
            Clean all targets without interactive confirmation.

    CrossShell for UNIX                                                    dskctl(1)
)";
    }

    static void PrintPreviewReport(const std::vector<CleanTarget>& targets, const DskctlConfig& config) {
        std::cout << "\nTargets Scheduled for Review:\n";
        DskctlLogger::AppendLogLine("Preview Report Start");

        for (const auto& target : targets) {
            if (!target.selected) continue;

            std::cout << "  [" << (target.destructive ? RED + std::string("DANGER") + RESET : GREEN + std::string("SAFE") + RESET)
                      << "] Target: " << target.name << " (Risk: " << target.risk << ")\n";
            DskctlLogger::AppendLogLine("  Target: " + target.name + " (" + target.risk + ")");

            if (target.flag == "--recycle-bin") {
                SHQUERYRBINFO rbInfo = { sizeof(SHQUERYRBINFO) };
                if (SUCCEEDED(SHQueryRecycleBinW(NULL, &rbInfo))) {
                    std::cout << "  * Recycle Bin Items: " << rbInfo.i64NumItems << " (" << StringHelper::FormatBytes(rbInfo.i64Size) << ")\n";
                    DskctlLogger::AppendLogLine("  * Recycle Bin items: " + std::to_string(rbInfo.i64NumItems));
                }
                continue;
            }

            for (const auto& rawPath : target.rawPaths) {
                fs::path p = StringHelper::ExpandEnvPath(rawPath);
                std::string resolvedText = p.string();
                std::error_code ec;
                if (!fs::exists(p, ec)) {
                    std::cout << "  * unresolved path: " << resolvedText << "\n";
                    DskctlLogger::AppendLogLine("  * unresolved path: " + resolvedText);
                    continue;
                }

                std::vector<std::string> items;
                size_t itemCount = 0;
                DiskCleanerEngine::CollectPreviewItems(p, items, itemCount);
                std::cout << "  * " << p.string() << " (" << itemCount << " items)\n";
                DskctlLogger::AppendLogLine("  * " + p.string() + " (" + std::to_string(itemCount) + " items)");

                size_t previewLimit = 5;
                for (size_t i = 0; i < std::min(previewLimit, items.size()); ++i) {
                    std::cout << "      - " << items[i] << "\n";
                    DskctlLogger::AppendLogLine("      - " + items[i]);
                }
                if (items.size() > previewLimit) {
                    std::cout << "      ... plus " << (items.size() - previewLimit) << " more\n";
                    DskctlLogger::AppendLogLine("      ... plus " + std::to_string(items.size() - previewLimit) + " more");
                }
            }
        }

        std::cout << "Mode: " << (config.dryRun ? "preview" : "execute") << "\n";
        DskctlLogger::AppendLogLine("Mode: " + std::string(config.dryRun ? "preview" : "execute"));
    }

    static void PrintSummary(const CleanSummary& summary, const DskctlConfig& config) {
        std::cout << BOLD << CYAN << "\n----------------------------------------\n";
        std::cout << "DSKCTL SUMMARY\n";
        std::cout << "----------------------------------------\n" << RESET;
        std::cout << "Total Space " << (config.dryRun ? "Reclaimable" : "Reclaimed") << ": "
                  << BOLD << GREEN << StringHelper::FormatBytes(summary.totalBytesFreed) << RESET << "\n";
        std::cout << "Total Objects Processed: " << summary.totalFilesRemoved << "\n";
        std::cout << "Skipped Entries: " << summary.totalSkipped << "\n";
        std::cout << "Errors: " << summary.totalErrors << "\n";
        std::cout << "Status: " << (summary.totalErrors == 0 ? (BOLD + std::string(GREEN) + "SUCCESS") : (BOLD + std::string(RED) + "COMPLETED WITH ERRORS")) << RESET << "\n";

        DskctlLogger::AppendLogLine("Summary: total reclaimed=" + std::to_string(summary.totalBytesFreed) + 
                                    ", processed=" + std::to_string(summary.totalFilesRemoved) + 
                                    ", skipped=" + std::to_string(summary.totalSkipped) + 
                                    ", errors=" + std::to_string(summary.totalErrors));

        if (config.outputFormat != OutputFormat::Default || !config.pipeCommand.empty()) {
            std::string text = (config.outputFormat == OutputFormat::Json) ?
                "{\"status\":\"" + std::string(summary.totalErrors == 0 ? "success" : "failure") + "\",\"bytes\":" + std::to_string(summary.totalBytesFreed) + ",\"objects\":" + std::to_string(summary.totalFilesRemoved) + ",\"skipped\":" + std::to_string(summary.totalSkipped) + ",\"errors\":" + std::to_string(summary.totalErrors) + "}\n" :
                (config.outputFormat == OutputFormat::Csv) ?
                "status,bytes,objects,skipped,errors\n" + std::string(summary.totalErrors == 0 ? "success," : "failure,") + std::to_string(summary.totalBytesFreed) + "," + std::to_string(summary.totalFilesRemoved) + "," + std::to_string(summary.totalSkipped) + "," + std::to_string(summary.totalErrors) + "\n" :
                "STATUS\tBYTES\tOBJECTS\tSKIPPED\tERRORS\n" + std::string(summary.totalErrors == 0 ? "success\t" : "failure\t") + std::to_string(summary.totalBytesFreed) + "\t" + std::to_string(summary.totalFilesRemoved) + "\t" + std::to_string(summary.totalSkipped) + "\t" + std::to_string(summary.totalErrors) + "\n";

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
    }
};
class DskctlApplication {
public:
    static bool PromptYesNo(const std::string& prompt) {
        std::cout << prompt << " [y/N]: ";
        std::string response;
        std::getline(std::cin, response);
        if (response.empty()) {
            return false;
        }
        return StringHelper::ToLowerCopy(response) == "y" || StringHelper::ToLowerCopy(response) == "yes";
    }

    static bool PromptConfirmDelete(const std::string& prompt) {
        std::cout << prompt << " Type 'CONFIRM' to continue: ";
        std::string response;
        std::getline(std::cin, response);
        return StringHelper::ToLowerCopy(response) == "confirm";
    }

    int Run(int argc, char* argv[]) const {
        ConsoleEnvironment::EnableVirtualTerminal();

        if (!SecurityInspector::IsCurrentUserAdmin()) {
            std::cerr << RED << BOLD << " [WARNING] " << RESET
                      << "dskctl requires elevated Administrator privileges.\n"
                      << "Please run this executable from an elevated terminal session.\n";
            return 1;
        }

        DskctlOptions opts;
        if (!opts.Parse(argc, argv)) {
            return 1;
        }

        if (opts.showHelp) {
            DskctlReporter::PrintHelp(argv[0], opts.targets);
            return 0;
        }

        std::cout << BOLD << CYAN << "========================================\n";
        std::cout << "   dskctl - Volume & Disk Control       \n";
        std::cout << "========================================\n" << RESET;
        if (opts.config.dryRun) {
            std::cout << YELLOW << BOLD << "[DRY-RUN MODE] Previewing cleanup actions only. No files will be modified.\n" << RESET;
        } else {
            std::cout << YELLOW << BOLD << "[EXECUTE MODE] Cleanup actions will be performed.\n" << RESET;
        }

        DskctlLogger::AppendLogLine("dskctl session started");
        DskctlReporter::PrintPreviewReport(opts.targets, opts.config);

        if (!opts.config.force && !opts.config.dryRun) {
            std::cout << "This action may remove files from temporary and cache locations.\n";
            if (!PromptYesNo("Continue?")) {
                std::cout << "Operation cancelled.\n";
                DskctlLogger::AppendLogLine("Operation cancelled by user");
                return 0;
            }
            if (!PromptConfirmDelete("This action is destructive and cannot be undone.")) {
                std::cout << "Operation cancelled.\n";
                DskctlLogger::AppendLogLine("Operation cancelled by user");
                return 0;
            }
        }

        CleanSummary summary;

        for (const auto& target : opts.targets) {
            if (!target.selected) continue;

            std::cout << BOLD << "[*] Executing Target: " << target.name << RESET << "\n";
            DskctlLogger::AppendLogLine("Executing target: " + target.name);

            if (target.flag == "--recycle-bin") {
                DiskCleanerEngine::CleanRecycleBin(opts.config, summary.totalBytesFreed);
                std::cout << GREEN << "    -> Recycle Bin purge requested\n" << RESET;
                DskctlLogger::AppendLogLine("Recycle Bin purge requested");
                continue;
            }

            uint64_t categoryBytes = 0;
            size_t categoryFiles = 0;
            size_t categorySkipped = 0;
            size_t categoryErrors = 0;

            for (const auto& rawPath : target.rawPaths) {
                fs::path p = StringHelper::ExpandEnvPath(rawPath);
                if (p.empty()) {
                    categorySkipped++;
                    continue;
                }

                uint64_t pathBytes = 0;
                size_t pathFiles = 0;
                size_t pathSkipped = 0;
                size_t pathErrors = 0;

                DiskCleanerEngine::CleanDirectory(p, opts.config, pathBytes, pathFiles, pathSkipped, pathErrors);

                categoryBytes += pathBytes;
                categoryFiles += pathFiles;
                categorySkipped += pathSkipped;
                categoryErrors += pathErrors;

                std::cout << "    Path: " << p.string() << "\n";
                std::cout << GREEN << "    -> " << (opts.config.dryRun ? "Would reclaim" : "Reclaimed") << ": "
                          << StringHelper::FormatBytes(pathBytes) << " (" << pathFiles << " items, "
                          << pathSkipped << " skipped, " << pathErrors << " errors)\n" << RESET;
                DskctlLogger::AppendLogLine("Path: " + p.string() + " => " + std::to_string(pathFiles) + " items, " + std::to_string(pathSkipped) + " skipped, " + std::to_string(pathErrors) + " errors");
            }

            summary.totalBytesFreed += categoryBytes;
            summary.totalFilesRemoved += categoryFiles;
            summary.totalSkipped += categorySkipped;
            summary.totalErrors += categoryErrors;
        }

        DskctlReporter::PrintSummary(summary, opts.config);

        return summary.totalErrors == 0 ? 0 : 1;
    }
};

int main(int argc, char* argv[]) {
    DskctlApplication app;
    return app.Run(argc, argv);
}