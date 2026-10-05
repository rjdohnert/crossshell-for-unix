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

#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <shlobj.h> // Required for SHGetKnownFolderPath
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <regex>
#include <chrono>
#include <ctime>
#include <sstream>
#include <filesystem>
#include <optional>
#include <cctype>
#include <algorithm>
#include <set>

#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Ole32.lib")

namespace fs = std::filesystem;

// ============================================================================
// 1. CONSOLE TERMINAL & COLOR MANAGEMENT
// ============================================================================
class ConsoleTerminal {
public:
    inline static const std::string Reset   = "\033[0m";
    inline static const std::string Bold    = "\033[1m";
    inline static const std::string Dim     = "\033[2m";
    inline static const std::string Red     = "\033[91m";
    inline static const std::string Green   = "\033[92m";
    inline static const std::string Yellow  = "\033[93m";
    inline static const std::string Blue    = "\033[94m";
    inline static const std::string Magenta = "\033[95m";
    inline static const std::string Cyan    = "\033[96m";
    inline static const std::string Gray    = "\033[90m";

    static void EnableVirtualTerminal() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != INVALID_HANDLE_VALUE) {
            DWORD dwMode = 0;
            if (GetConsoleMode(hOut, &dwMode)) {
                dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
                SetConsoleMode(hOut, dwMode);
            }
        }
    }

    static std::string FormatTypeBadge(bool isDir, bool isSym) {
        if (isDir)      return Blue + "[DIR]" + Reset;
        if (isSym) return Magenta + "[SYM]" + Reset;
        return Green + "[FILE]" + Reset;
    }
};

// ============================================================================
// 2. DATA FORMATTERS & ATTRIBUTE INSPECTORS
// ============================================================================
class SizeFormatter {
public:
    static std::optional<uintmax_t> Parse(const std::string& str) {
        if (str.empty()) return std::nullopt;
        std::string s = str;
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);

        double multiplier = 1.0;
        size_t numEnd = 0;
        while (numEnd < s.size() && (std::isdigit(static_cast<unsigned char>(s[numEnd])) || s[numEnd] == '.')) {
            numEnd++;
        }
        if (numEnd == 0) return std::nullopt;

        double val = 0.0;
        try {
            val = std::stod(s.substr(0, numEnd));
        } catch (...) {
            return std::nullopt;
        }

        std::string unit = s.substr(numEnd);
        if (unit.empty() || unit == "B" || unit == "BYTES") multiplier = 1.0;
        else if (unit == "K" || unit == "KB") multiplier = 1024.0;
        else if (unit == "M" || unit == "MB") multiplier = 1024.0 * 1024.0;
        else if (unit == "G" || unit == "GB") multiplier = 1024.0 * 1024.0 * 1024.0;
        else if (unit == "T" || unit == "TB") multiplier = 1024.0 * 1024.0 * 1024.0 * 1024.0;
        else return std::nullopt;

        return static_cast<uintmax_t>(val * multiplier);
    }

    static std::string Format(uintmax_t bytes) {
        const char* units[] = {"B", "KB", "MB", "GB", "TB"};
        int idx = 0;
        double size = static_cast<double>(bytes);
        while (size >= 1024.0 && idx < 4) {
            size /= 1024.0;
            idx++;
        }
        std::ostringstream ss;
        if (idx == 0) ss << bytes << " B";
        else ss << std::fixed << std::setprecision(1) << size << " " << units[idx];
        return ss.str();
    }
};

class DateTimeFormatter {
public:
    static std::chrono::system_clock::time_point ToSystemTime(fs::file_time_type ftime) {
        return std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now()
        );
    }

    static std::optional<std::chrono::system_clock::time_point> Parse(const std::string& str) {
        if (str.empty()) return std::nullopt;
        auto now = std::chrono::system_clock::now();

        char lastChar = static_cast<char>(std::tolower(static_cast<unsigned char>(str.back())));
        if (lastChar == 'd' || lastChar == 'h' || lastChar == 'm' || lastChar == 's') {
            try {
                long long val = std::stoll(str.substr(0, str.size() - 1));
                if (lastChar == 'd') return now - std::chrono::hours(val * 24);
                if (lastChar == 'h') return now - std::chrono::hours(val);
                if (lastChar == 'm') return now - std::chrono::minutes(val);
                if (lastChar == 's') return now - std::chrono::seconds(val);
            } catch (...) {
                return std::nullopt;
            }
        }

        std::tm tm = {};
        std::istringstream ss(str);
        if (str.find(':') != std::string::npos) {
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
        } else {
            ss >> std::get_time(&tm, "%Y-%m-%d");
        }

        if (ss.fail()) return std::nullopt;
        std::time_t tt = std::mktime(&tm);
        return std::chrono::system_clock::from_time_t(tt);
    }

    static std::string Format(fs::file_time_type ftime) {
        auto sctp = ToSystemTime(ftime);
        std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
        std::tm* local_tm = std::localtime(&tt);
        char buf[32];
        if (local_tm) {
            std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", local_tm);
            return std::string(buf);
        }
        return "N/A";
    }
};

class AttributeInspector {
public:
    static std::string GetAttributesString(const fs::path& p, DWORD* rawAttr = nullptr) {
        DWORD attr = GetFileAttributesW(p.c_str());
        if (rawAttr) *rawAttr = attr;
        if (attr == INVALID_FILE_ATTRIBUTES) return "-------";

        std::string s = "";
        s += (attr & FILE_ATTRIBUTE_READONLY) ? 'R' : '-';
        s += (attr & FILE_ATTRIBUTE_HIDDEN)   ? 'H' : '-';
        s += (attr & FILE_ATTRIBUTE_SYSTEM)   ? 'S' : '-';
        s += (attr & FILE_ATTRIBUTE_ARCHIVE)  ? 'A' : '-';
        s += (attr & FILE_ATTRIBUTE_COMPRESSED) ? 'C' : '-';
        s += (attr & FILE_ATTRIBUTE_ENCRYPTED)  ? 'E' : '-';
        return s;
    }
};

// ============================================================================
// 3. KNOWN FOLDERS LOCATOR
// ============================================================================
class KnownFolderLocator {
public:
    static std::optional<fs::path> GetKnownFolderPath(REFKNOWNFOLDERID folderId) {
        PWSTR path = nullptr;
        HRESULT hr = SHGetKnownFolderPath(folderId, KF_FLAG_DEFAULT, NULL, &path);
        if (SUCCEEDED(hr) && path != nullptr) {
            fs::path result(path);
            CoTaskMemFree(path);
            return result;
        }
        return std::nullopt;
    }

    static void PopulateUserCommonPaths(std::vector<fs::path>& targetPaths) {
        auto downloads = GetKnownFolderPath(FOLDERID_Downloads);
        auto docs      = GetKnownFolderPath(FOLDERID_Documents);
        auto desktop   = GetKnownFolderPath(FOLDERID_Desktop);
        auto pictures  = GetKnownFolderPath(FOLDERID_Pictures);
        auto music     = GetKnownFolderPath(FOLDERID_Music);
        auto videos    = GetKnownFolderPath(FOLDERID_Videos);

        std::error_code ec;
        auto addIfExists = [&](const std::optional<fs::path>& p) {
            if (p && fs::exists(*p, ec)) {
                if (std::find(targetPaths.begin(), targetPaths.end(), *p) == targetPaths.end()) {
                    targetPaths.push_back(*p);
                }
            }
        };

        addIfExists(downloads);
        addIfExists(docs);
        addIfExists(desktop);
        addIfExists(pictures);
        addIfExists(music);
        addIfExists(videos);
    }
};

// ============================================================================
// 4. PATTERN & REGEX MATCHER
// ============================================================================
class PatternMatcher {
private:
    std::unique_ptr<std::regex> m_regex;

    static std::string WildcardToRegex(const std::string& pattern, bool exact) {
        if (pattern.empty()) return ".*";
        std::string regexStr = exact ? "^" : "";

        bool hasWildcard = (pattern.find('*') != std::string::npos || pattern.find('?') != std::string::npos);
        if (!exact && !hasWildcard) {
            regexStr += ".*";
        }

        for (char c : pattern) {
            switch (c) {
                case '*': regexStr += ".*"; break;
                case '?': regexStr += "."; break;
                case '.': case '+': case '(': case ')':
                case '[': case ']': case '{': case '}':
                case '^': case '$': case '|': case '\\':
                    regexStr += "\\";
                    regexStr += c;
                    break;
                default:
                    regexStr += c;
            }
        }

        if (!exact && !hasWildcard) regexStr += ".*";
        if (exact) regexStr += "$";
        return regexStr;
    }

public:
    bool Initialize(const std::string& pattern, bool isRegex, bool exact, bool caseInsensitive) {
        if (pattern.empty()) {
            m_regex.reset();
            return true;
        }

        std::string regStr = isRegex ? pattern : WildcardToRegex(pattern, exact);
        auto flags = std::regex::ECMAScript;
        if (caseInsensitive) flags |= std::regex::icase;

        try {
            m_regex = std::make_unique<std::regex>(regStr, flags);
            return true;
        } catch (const std::regex_error& e) {
            std::cerr << ConsoleTerminal::Red << "Regex compilation error: " << ConsoleTerminal::Reset << e.what() << "\n";
            return false;
        }
    }

    bool HasPattern() const {
        return m_regex != nullptr;
    }

    bool Matches(const std::string& filename) const {
        if (!m_regex) return true;
        return std::regex_match(filename, *m_regex);
    }
};

// ============================================================================
// 5. SEARCH OPTIONS & PARSER
// ============================================================================
struct SearchOptions {
    std::vector<fs::path> targetPaths;
    std::string namePattern = "";
    std::vector<std::string> extensions;
    char typeFilter = 'a'; // 'a' = all, 'f' = file, 'd' = directory
    bool isRegex = false;
    bool caseInsensitive = true;
    bool exactMatch = false;
    int maxDepth = -1; // -1 = infinite

    // Size filters (in bytes)
    std::optional<uintmax_t> minSize;
    std::optional<uintmax_t> maxSize;

    // Time filters
    std::optional<std::chrono::system_clock::time_point> modifiedAfter;
    std::optional<std::chrono::system_clock::time_point> modifiedBefore;

    // Windows attributes
    bool includeHidden = true;
    bool hiddenOnly = false;
    bool readonlyOnly = false;
    bool systemOnly = false;

    // Display options
    bool summaryOnly = false;
    bool bareOutput = false;
    bool showAttributes = true;
    bool searchUserDirs = false;
};

class OptionParser {
public:
    static void PrintHelp() {
        std::cout <<
R"(search(1)                 CrossShell for UNIX Reference Manual                  search(1)

NAME
    search - advanced recursive file and metadata search engine for Windows

SYNOPSIS
    search [PATHS...] [NAME_PATTERN] [OPTIONS]

DESCRIPTION
    Recursively searches files and directories matching names, wildcards,
    regular expressions, file extensions, sizes, timestamps, and Windows
    file attributes. If no target directory is specified, the current
    directory (".") is searched.

OPTIONS
    Path and User Directory:
        [PATHS...]
            One or more directories or drives to search (e.g., ".", "C:\").
        -u, --user
            Search user personal directories (Downloads, Documents, Desktop,
            Pictures, Music, Videos, including OneDrive folders).

    Name and Pattern Matching:
        -n, --name PATTERN
            Search pattern with wildcards ('*' and '?').
        -r, --regex REGEX
            Search name using ECMAScript regular expressions.
        -e, --ext EXTS
            Comma-separated list of extensions (e.g., "cpp,h", "pdf").
        -x, --exact
            Enforce exact full-name matching instead of substring.
        -s, --case-sensitive
            Enable case-sensitive matching (default: case-insensitive).

    Metadata and Size Filters:
        -t, --type TYPE
            Filter entry type: (f)ile, (d)irectory, (a)ll.
        --min-size SIZE
            Find files >= size (e.g., 512B, 100KB, 50MB, 2.5GB, 1TB).
        --max-size SIZE
            Find files <= size.
        --empty
            Find empty files (0 bytes) or empty directories.

    Date and Time Filters:
        --after DATE
            Modified on/after date (YYYY-MM-DD) or relative duration.
        --before DATE
            Modified on/before date (YYYY-MM-DD).
        --newer-than DURATION
            Modified within duration (e.g. "30m", "12h", "7d", "30d").
        --older-than DURATION
            Modified prior to duration.

    Windows Attributes and Traversal:
        -d, --depth N
            Limit recursion depth (0 = target directory only).
        --hidden
            Match hidden files and directories only.
        --no-hidden
            Skip hidden files and directories.
        --readonly
            Match read-only files only.
        --system
            Match system files only.

    Output and Display:
        -b, --bare
            Print only matching file paths (ideal for piping).
        --summary
            Print summary statistics only.
        -h, --help, /?
            Display this comprehensive reference manual and exit.

EXAMPLES
    search -u "invoice*.pdf"
        Find downloads matching pattern across user directories.

    search budget
        Find file or folder named 'budget' in current dir and subdirs.

    search --user --min-size 100MB --newer-than 7d
        Search user folders for large files modified this week.

    search C:\Engine D:\Plugins -e cpp,h,hpp
        Find all C++ source and header files in two project folders.

    search D:\Logs -n "*.log" --before 2024-01-01
        Find all log files modified before 2024-01-01.

    search C:\Config -e json,ini --readonly --bare > config.txt
        Output bare paths of read-only configs to a file.

EXIT STATUS
    0   Success.
    1   Invalid options or search execution failure.

    CrossShell for UNIX                                                 search(1)
)";
    }

    bool Parse(int argc, char* argv[], SearchOptions& opt) const {
        std::vector<std::string> positionalArgs;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "-h" || arg == "--help" || arg == "/?") {
                PrintHelp();
                return false;
            } else if (arg == "-u" || arg == "--user") {
                opt.searchUserDirs = true;
            } else if ((arg == "-n" || arg == "--name") && i + 1 < argc) {
                opt.namePattern = argv[++i];
                opt.isRegex = false;
            } else if ((arg == "-r" || arg == "--regex") && i + 1 < argc) {
                opt.namePattern = argv[++i];
                opt.isRegex = true;
            } else if ((arg == "-e" || arg == "--ext") && i + 1 < argc) {
                std::string extList = argv[++i];
                std::stringstream ss(extList);
                std::string item;
                while (std::getline(ss, item, ',')) {
                    if (!item.empty()) {
                        if (item[0] != '.') item = "." + item;
                        std::transform(item.begin(), item.end(), item.begin(), ::tolower);
                        opt.extensions.push_back(item);
                    }
                }
            } else if ((arg == "-t" || arg == "--type") && i + 1 < argc) {
                std::string val = argv[++i];
                char c = static_cast<char>(std::tolower(static_cast<unsigned char>(val[0])));
                if (c == 'f' || c == 'd' || c == 'a') opt.typeFilter = c;
                else {
                    std::cerr << ConsoleTerminal::Red << "Error: " << ConsoleTerminal::Reset << "Invalid type filter: " << val << " (Use f, d, or all)\n";
                    return false;
                }
            } else if (arg == "-x" || arg == "--exact") {
                opt.exactMatch = true;
            } else if (arg == "-s" || arg == "--case-sensitive") {
                opt.caseInsensitive = false;
            } else if ((arg == "-d" || arg == "--depth") && i + 1 < argc) {
                opt.maxDepth = std::stoi(argv[++i]);
            } else if (arg == "--min-size" && i + 1 < argc) {
                opt.minSize = SizeFormatter::Parse(argv[++i]);
                if (!opt.minSize) {
                    std::cerr << ConsoleTerminal::Red << "Error: " << ConsoleTerminal::Reset << "Invalid size format: " << argv[i] << "\n";
                    return false;
                }
            } else if (arg == "--max-size" && i + 1 < argc) {
                opt.maxSize = SizeFormatter::Parse(argv[++i]);
                if (!opt.maxSize) {
                    std::cerr << ConsoleTerminal::Red << "Error: " << ConsoleTerminal::Reset << "Invalid size format: " << argv[i] << "\n";
                    return false;
                }
            } else if (arg == "--empty") {
                opt.minSize = 0;
                opt.maxSize = 0;
            } else if ((arg == "--after" || arg == "--newer-than") && i + 1 < argc) {
                opt.modifiedAfter = DateTimeFormatter::Parse(argv[++i]);
                if (!opt.modifiedAfter) {
                    std::cerr << ConsoleTerminal::Red << "Error: " << ConsoleTerminal::Reset << "Invalid date/duration: " << argv[i] << "\n";
                    return false;
                }
            } else if ((arg == "--before" || arg == "--older-than") && i + 1 < argc) {
                opt.modifiedBefore = DateTimeFormatter::Parse(argv[++i]);
                if (!opt.modifiedBefore) {
                    std::cerr << ConsoleTerminal::Red << "Error: " << ConsoleTerminal::Reset << "Invalid date/duration: " << argv[i] << "\n";
                    return false;
                }
            } else if (arg == "--hidden") {
                opt.hiddenOnly = true;
            } else if (arg == "--no-hidden") {
                opt.includeHidden = false;
            } else if (arg == "--readonly") {
                opt.readonlyOnly = true;
            } else if (arg == "--system") {
                opt.systemOnly = true;
            } else if (arg == "-b" || arg == "--bare") {
                opt.bareOutput = true;
            } else if (arg == "--summary") {
                opt.summaryOnly = true;
            } else if (arg[0] != '-') {
                positionalArgs.push_back(arg);
            } else {
                std::cerr << ConsoleTerminal::Red << "Unknown option: " << ConsoleTerminal::Reset << arg 
                          << " (Run " << ConsoleTerminal::Yellow << "search --help" << ConsoleTerminal::Reset << " for syntax)\n";
                return false;
            }
        }

        if (opt.searchUserDirs) {
            KnownFolderLocator::PopulateUserCommonPaths(opt.targetPaths);
        }

        for (const auto& pos : positionalArgs) {
            std::error_code ec;
            if (fs::exists(pos, ec)) {
                opt.targetPaths.push_back(fs::path(pos));
            } else {
                if (opt.namePattern.empty()) {
                    opt.namePattern = pos;
                } else {
                    opt.targetPaths.push_back(fs::path(pos));
                }
            }
        }

        if (opt.targetPaths.empty()) {
            opt.targetPaths.push_back(fs::current_path());
        }

        return true;
    }
};

// ============================================================================
// 6. SEARCH REPORTER & OUTPUT PRESENTER
// ============================================================================
class SearchReporter {
public:
    static void PrintBanner() {
        std::cout << ConsoleTerminal::Bold << "search" << ConsoleTerminal::Reset 
                  << " (C++17 Windows File Search Engine)\n"
                  << "Run " << ConsoleTerminal::Yellow << "search --help" << ConsoleTerminal::Reset << " or " 
                  << ConsoleTerminal::Yellow << "search /?" << ConsoleTerminal::Reset << " for full documentation and options.\n\n"
                  << "Searching current directory recursively...\n\n";
    }

    static void PrintHeader(const SearchOptions& opt) {
        if (opt.bareOutput || opt.summaryOnly) return;

        std::cout << ConsoleTerminal::Bold << ConsoleTerminal::Cyan << "SEARCH TARGETS:" << ConsoleTerminal::Reset << "\n";
        for (const auto& p : opt.targetPaths) {
            std::error_code ec;
            std::cout << "  -> " << fs::absolute(p, ec).string() << "\n";
        }
        std::cout << "\n";

        std::cout << ConsoleTerminal::Dim 
                  << std::left << std::setw(7)  << "TYPE"
                  << std::left << std::setw(11) << "SIZE"
                  << std::left << std::setw(8)  << "ATTR"
                  << std::left << std::setw(21) << "MODIFIED"
                  << "PATH" << ConsoleTerminal::Reset << "\n";
        std::cout << std::string(85, '-') << "\n";
    }

    static void PrintResult(const fs::directory_entry& entry, bool isDir, bool isReg, bool isSym,
                            uintmax_t fsize, const std::string& attrStr, fs::file_time_type ftime,
                            const SearchOptions& opt) {
        if (opt.bareOutput) {
            std::cout << entry.path().string() << "\n";
            return;
        }

        if (opt.summaryOnly) return;

        std::string typeBadge = ConsoleTerminal::FormatTypeBadge(isDir, isSym);
        std::string sizeStr = isDir ? "-" : SizeFormatter::Format(fsize);
        std::string timeStr = DateTimeFormatter::Format(ftime);

        std::cout << std::left << std::setw(16) << typeBadge
                  << std::left << std::setw(11) << sizeStr
                  << std::left << std::setw(8)  << attrStr
                  << std::left << std::setw(21) << timeStr
                  << (isDir ? ConsoleTerminal::Bold : "") << entry.path().string() << ConsoleTerminal::Reset
                  << "\n";
    }

    static void PrintSummary(size_t totalScanned, size_t matchedFiles, size_t matchedDirs,
                             uintmax_t matchedBytes, double durationMs, const SearchOptions& opt) {
        if (opt.bareOutput) return;

        std::cout << std::string(85, '-') << "\n"
                  << ConsoleTerminal::Bold << "Summary: " << ConsoleTerminal::Reset
                  << ConsoleTerminal::Green << matchedFiles << ConsoleTerminal::Reset << " files (" << SizeFormatter::Format(matchedBytes) << "), "
                  << ConsoleTerminal::Blue << matchedDirs << ConsoleTerminal::Reset << " directories matched "
                  << ConsoleTerminal::Dim << "(Scanned " << totalScanned << " items in " 
                  << std::fixed << std::setprecision(1) << durationMs << " ms)" << ConsoleTerminal::Reset << "\n";
    }
};

// ============================================================================
// 7. SEARCH ENGINE
// ============================================================================
class SearchEngine {
private:
    PatternMatcher m_matcher;

public:
    int Execute(const SearchOptions& opt) {
        if (!m_matcher.Initialize(opt.namePattern, opt.isRegex, opt.exactMatch, opt.caseInsensitive)) {
            return 1;
        }

        SearchReporter::PrintHeader(opt);

        size_t totalScanned = 0;
        size_t matchedFiles = 0;
        size_t matchedDirs  = 0;
        uintmax_t matchedBytes = 0;
        auto startTime = std::chrono::high_resolution_clock::now();

        for (const auto& root : opt.targetPaths) {
            std::error_code ec;
            if (!fs::exists(root, ec)) {
                std::cerr << ConsoleTerminal::Red << "Warning: " << ConsoleTerminal::Reset << "Path not found: " << root.string() << "\n";
                continue;
            }

            auto it_opts = fs::directory_options::skip_permission_denied;
            auto iter = fs::recursive_directory_iterator(root, it_opts, ec);
            auto end = fs::recursive_directory_iterator();

            for (; iter != end; iter.increment(ec)) {
                if (ec) {
                    ec.clear();
                    continue;
                }

                totalScanned++;

                if (opt.maxDepth >= 0 && iter.depth() > opt.maxDepth) {
                    iter.pop();
                    continue;
                }

                const auto& entry = *iter;
                bool isDir = entry.is_directory(ec);
                bool isReg = entry.is_regular_file(ec);
                bool isSym = entry.is_symlink(ec);

                // 1. Filter: Type
                if (opt.typeFilter == 'f' && !isReg) continue;
                if (opt.typeFilter == 'd' && !isDir) continue;

                // 2. Filter: Windows Attributes
                DWORD rawAttr = 0;
                std::string attrStr = AttributeInspector::GetAttributesString(entry.path(), &rawAttr);
                bool isHidden = (rawAttr != INVALID_FILE_ATTRIBUTES) && (rawAttr & FILE_ATTRIBUTE_HIDDEN);
                bool isReadonly = (rawAttr != INVALID_FILE_ATTRIBUTES) && (rawAttr & FILE_ATTRIBUTE_READONLY);
                bool isSys = (rawAttr != INVALID_FILE_ATTRIBUTES) && (rawAttr & FILE_ATTRIBUTE_SYSTEM);

                if (!opt.includeHidden && isHidden) continue;
                if (opt.hiddenOnly && !isHidden) continue;
                if (opt.readonlyOnly && !isReadonly) continue;
                if (opt.systemOnly && !isSys) continue;

                // 3. Filter: Extensions
                if (!opt.extensions.empty()) {
                    std::string fileExt = entry.path().extension().string();
                    std::transform(fileExt.begin(), fileExt.end(), fileExt.begin(), ::tolower);
                    bool extMatch = false;
                    for (const auto& ext : opt.extensions) {
                        if (fileExt == ext) {
                            extMatch = true;
                            break;
                        }
                    }
                    if (!extMatch) continue;
                }

                // 4. Filter: Name / Regex / Wildcard
                if (m_matcher.HasPattern()) {
                    std::string filename = entry.path().filename().string();
                    if (!m_matcher.Matches(filename)) continue;
                }

                // 5. Filter: Size
                uintmax_t fsize = 0;
                if (isReg) {
                    fsize = entry.file_size(ec);
                    if (ec) { fsize = 0; ec.clear(); }
                    if (opt.minSize && fsize < *opt.minSize) continue;
                    if (opt.maxSize && fsize > *opt.maxSize) continue;
                } else if (isDir && opt.minSize && *opt.minSize == 0 && opt.maxSize && *opt.maxSize == 0) {
                    auto subIt = fs::directory_iterator(entry.path(), it_opts, ec);
                    if (subIt != fs::directory_iterator()) continue;
                }

                // 6. Filter: Timestamps
                auto ftime = entry.last_write_time(ec);
                if (!ec) {
                    auto sysTime = DateTimeFormatter::ToSystemTime(ftime);
                    if (opt.modifiedAfter && sysTime < *opt.modifiedAfter) continue;
                    if (opt.modifiedBefore && sysTime > *opt.modifiedBefore) continue;
                }

                // Metric tallies
                if (isDir) matchedDirs++;
                if (isReg) {
                    matchedFiles++;
                    matchedBytes += fsize;
                }

                // Output Result
                SearchReporter::PrintResult(entry, isDir, isReg, isSym, fsize, attrStr, ftime, opt);
            }
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        double durationMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

        SearchReporter::PrintSummary(totalScanned, matchedFiles, matchedDirs, matchedBytes, durationMs, opt);
        return 0;
    }
};

// ============================================================================
// 8. APPLICATION CONTROLLER & ENTRY POINT
// ============================================================================
class SearchApplication {
private:
    OptionParser m_parser;
    SearchEngine m_engine;

public:
    int Run(int argc, char* argv[]) {
        ConsoleTerminal::EnableVirtualTerminal();

        if (argc == 1) {
            SearchReporter::PrintBanner();
        }

        SearchOptions opt;
        if (!m_parser.Parse(argc, argv, opt)) {
            return (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help" || std::string(argv[1]) == "/?")) ? 0 : 1;
        }

        return m_engine.Execute(opt);
    }
};

int main(int argc, char* argv[]) {
    SearchApplication app;
    return app.Run(argc, argv);
}