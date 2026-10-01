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
#include <aclapi.h>
#include <sddl.h>
#include <iostream>
#include <vector>
#include <string>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <algorithm>
#include <memory>
#include <unordered_set>

#pragma comment(lib, "advapi32.lib")

namespace fs = std::filesystem;

static std::string pathToUtf8(const fs::path& p) {
    const std::wstring& ws = p.native();
    if (ws.empty()) return {};
    int req = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (req <= 1) return {};
    std::string out(static_cast<size_t>(req) - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), req, nullptr, nullptr);
    return out;
}

static std::string filenameToUtf8(const fs::path& p) {
    std::wstring ws = p.filename().native();
    if (ws.empty()) ws = p.native();
    int req = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (req <= 1) return {};
    std::string out(static_cast<size_t>(req) - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), req, nullptr, nullptr);
    return out;
}

static std::string extensionToUtf8(const fs::path& p) {
    std::wstring ws = p.extension().native();
    if (ws.empty()) return {};
    int req = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (req <= 1) return {};
    std::string out(static_cast<size_t>(req) - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, out.data(), req, nullptr, nullptr);
    return out;
}

// ============================================================================
// Class: ColorTheme
// Manages terminal color profiles: Blue, Cyan, Orange, Pink, Red, Purple.
// ============================================================================
class ColorTheme {
public:
    inline static const std::string RESET        = "\033[0m";
    inline static const std::string BOLD         = "\033[1m";
    inline static const std::string DIR_BLUE     = "\033[38;2;68;142;255m";  // Directories: Blue
    inline static const std::string EXE_CYAN     = "\033[38;2;0;235;255m";   // .exe/bin: Cyan
    inline static const std::string SRC_ORANGE   = "\033[38;2;255;140;0m";   // Sources: Orange
    inline static const std::string IMG_PINK     = "\033[38;2;255;105;180m"; // Images: Pink
    inline static const std::string ARC_RED      = "\033[38;2;255;70;70m";   // Archives: Red
    inline static const std::string MEDIA_PURPLE = "\033[38;2;186;85;211m";  // Multimedia: Purple
    inline static const std::string LINK_BRIGHT  = "\033[38;2;120;255;120m"; // Reparse/Links: Bright Green
    inline static const std::string ATTR_COLOR   = "\033[38;2;170;170;170m";

    static std::string classify(const fs::path& path, DWORD attr, bool isLink) {
        if (attr & FILE_ATTRIBUTE_DIRECTORY) {
            return DIR_BLUE;
        }
        if (isLink) {
            return LINK_BRIGHT;
        }

        std::string ext = extensionToUtf8(path);
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { 
            return static_cast<char>(::tolower(c)); 
        });

        // Executables -> Cyan
        static const std::unordered_set<std::string> exes = {
            ".exe", ".bat", ".cmd", ".com", ".ps1", ".msi", ".scr", ".vbs"
        };
        if (exes.count(ext)) return EXE_CYAN;

        // Source Files -> Orange
        static const std::unordered_set<std::string> sources = {
            ".c", ".cpp", ".cxx", ".cc", ".h", ".hpp", ".cs", ".rs", ".go", 
            ".py", ".java", ".js", ".ts", ".html", ".css", ".sql", ".sh", 
            ".asm", ".json", ".xml", ".yaml", ".yml", ".toml", ".lua"
        };
        if (sources.count(ext)) return SRC_ORANGE;

        // Image Files -> Pink
        static const std::unordered_set<std::string> images = {
            ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".svg", ".webp", ".ico", 
            ".tiff", ".tif", ".psd", ".raw"
        };
        if (images.count(ext)) return IMG_PINK;

        // Archive Files -> Red
        static const std::unordered_set<std::string> archives = {
            ".zip", ".tar", ".gz", ".7z", ".rar", ".bz2", ".xz", ".cab", 
            ".iso", ".tgz", ".lz", ".zst"
        };
        if (archives.count(ext)) return ARC_RED;

        // Multimedia Files -> Purple
        static const std::unordered_set<std::string> media = {
            ".mp3", ".mp4", ".wav", ".mkv", ".avi", ".flac", ".mov", ".wmv", 
            ".ogg", ".m4a", ".aac", ".webm", ".wma", ".mpg", ".mpeg"
        };
        if (media.count(ext)) return MEDIA_PURPLE;

        return RESET;
    }
};

// ============================================================================
// Class: WindowsSecurityHelper
// Resolves actual Windows NT SIDs, Owner, Domain, and DACL (+) Status.
// ============================================================================
class WindowsSecurityHelper {
public:
    struct SecurityInfo {
        std::string owner = "-";
        std::string domain = "-";
        bool hasCustomAcl = false;
    };

    static std::string toUtf8(const std::wstring& value) {
        if (value.empty()) {
            return {};
        }

        int required = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
        if (required <= 0) {
            return {};
        }

        std::string out(static_cast<size_t>(required) - 1, '\0');
        WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), required, nullptr, nullptr);
        return out;
    }

    static SecurityInfo query(const fs::path& path) {
        SecurityInfo info;
        PSECURITY_DESCRIPTOR pSD = nullptr;
        PSID pOwnerSid = nullptr;
        PACL pDacl = nullptr;

        DWORD res = GetNamedSecurityInfoW(
            path.c_str(),
            SE_FILE_OBJECT,
            OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
            &pOwnerSid,
            nullptr,
            &pDacl,
            nullptr,
            &pSD
        );

        if (res == ERROR_SUCCESS && pSD != nullptr) {
            if (pOwnerSid && IsValidSid(pOwnerSid)) {
                WCHAR name[256];
                WCHAR dom[256];
                DWORD nameLen = 256;
                DWORD domLen = 256;
                SID_NAME_USE use;

                if (LookupAccountSidW(nullptr, pOwnerSid, name, &nameLen, dom, &domLen, &use)) {
                    std::wstring wsOwner(name);
                    std::wstring wsDomain(dom);
                    info.owner = toUtf8(wsOwner);
                    info.domain = toUtf8(wsDomain);
                }
            }

            if (pDacl != nullptr && pDacl->AceCount > 0) {
                info.hasCustomAcl = true;
            }

            LocalFree(pSD);
        }
        return info;
    }
};

// ============================================================================
// Class: FileItem
// Represents a filesystem node with Windows attributes (no POSIX emulation).
// ============================================================================
class FileItem {
public:
    fs::path path;
    std::string name;
    DWORD attributes = 0;
    uint64_t size = 0;
    fs::file_time_type lastWriteTime;
    bool isSymlink = false;
    bool isDirectory = false;
    WindowsSecurityHelper::SecurityInfo secInfo;

    FileItem(const fs::path& p, bool fetchSecInfo) : path(p), name(filenameToUtf8(p)) {
        if (name.empty()) name = pathToUtf8(p);

        std::error_code ec;
        isSymlink = fs::is_symlink(p, ec);
        isDirectory = fs::is_directory(p, ec);

        attributes = GetFileAttributesW(p.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            attributes = 0;
        }

        if (!isDirectory && !isSymlink) {
            size = fs::file_size(p, ec);
            if (ec) size = 0;
        }

        lastWriteTime = fs::last_write_time(p, ec);

        if (fetchSecInfo) {
            secInfo = WindowsSecurityHelper::query(p);
        }
    }

    // Authentic Windows Attribute representation: [d][r][a][h][s][c][e][l][+]
    std::string getWindowsModeString() const {
        std::string mode = "---------";
        if (attributes & FILE_ATTRIBUTE_DIRECTORY)     mode[0] = 'd';
        if (attributes & FILE_ATTRIBUTE_READONLY)      mode[1] = 'r';
        if (attributes & FILE_ATTRIBUTE_ARCHIVE)       mode[2] = 'a';
        if (attributes & FILE_ATTRIBUTE_HIDDEN)        mode[3] = 'h';
        if (attributes & FILE_ATTRIBUTE_SYSTEM)        mode[4] = 's';
        if (attributes & FILE_ATTRIBUTE_COMPRESSED)    mode[5] = 'c';
        if (attributes & FILE_ATTRIBUTE_ENCRYPTED)     mode[6] = 'e';
        if (attributes & FILE_ATTRIBUTE_REPARSE_POINT) mode[7] = 'l';
        if (secInfo.hasCustomAcl)                      mode[8] = '+'; // HP-UX extended ACL marker
        return mode;
    }

    std::string getFormattedTimestamp() const {
        using namespace std::chrono;
        auto sctp = time_point_cast<system_clock::duration>(
            lastWriteTime - fs::file_time_type::clock::now() + system_clock::now()
        );
        std::time_t tt = system_clock::to_time_t(sctp);
        std::tm tmVal;
        localtime_s(&tmVal, &tt);

        auto now = system_clock::to_time_t(system_clock::now());
        double diff = std::difftime(now, tt);

        char buf[32];
        // HP-UX timestamp standard: MMM dd HH:mm (or YYYY if > 6 months old)
        if (diff > 15552000 || diff < -15552000) {
            std::strftime(buf, sizeof(buf), "%b %d  %Y", &tmVal);
        } else {
            std::strftime(buf, sizeof(buf), "%b %d %H:%M", &tmVal);
        }
        return std::string(buf);
    }
};

// ============================================================================
// Class: ListingOptions
// Command line configuration flags for HP-UX ls.
// ============================================================================
class ListingOptions {
public:
    bool all = false;          // -a: List all entries (. and ..)
    bool almostAll = false;    // -A: List entries except . and ..
    bool longFormat = false;   // -l: Detailed listing with Windows modes
    bool typeIndicator = false;// -F: Append /, *, @
    bool recursive = false;    // -R: Recursive subtree listing
    bool reverseSort = false;  // -r: Reverse sort order
    bool sortByTime = false;   // -t: Sort by modification time
    bool sortBySize = false;   // -S: Sort by file size
    bool singleColumn = false; // -1: Single column output
    bool color = true;         // ANSI TrueColor support
    std::string outputFormat;  // empty, json, csv, or table
    std::vector<std::string> targets;
};

// ============================================================================
// Class: ListingEngine
// Sorts, formats, and displays directories and multi-column tables.
// ============================================================================
class ListingEngine {
private:
    ListingOptions options;

    int getTerminalWidth() const {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
            return csbi.srWindow.Right - csbi.srWindow.Left + 1;
        }
        return 80;
    }

    std::string jsonEscape(const std::string& value) const {
        std::ostringstream out;
        for (unsigned char ch : value) {
            switch (ch) {
                case '\\': out << "\\\\"; break;
                case '"': out << "\\\""; break;
                case '\n': out << "\\n"; break;
                case '\r': out << "\\r"; break;
                case '\t': out << "\\t"; break;
                default:
                    if (ch < 0x20) {
                        out << "\\u00" << std::hex << std::setw(2) << std::setfill('0')
                            << static_cast<int>(ch) << std::dec << std::setfill(' ');
                    } else {
                        out << static_cast<char>(ch);
                    }
                    break;
            }
        }
        return out.str();
    }

    std::string csvEscape(const std::string& value) const {
        if (value.find_first_of(",\"\n\r") == std::string::npos) {
            return value;
        }
        std::string escaped; escaped.reserve(value.size() + 2);
        escaped += '"';
        for (char ch : value) {
            if (ch == '"') escaped += "\"\"";
            else escaped += ch;
        }
        escaped += '"';
        return escaped;
    }

    std::string getTypeName(const FileItem& item) const {
        if (item.isDirectory) return "directory";
        if (item.isSymlink) return "symlink";
        if (item.attributes & FILE_ATTRIBUTE_REPARSE_POINT) return "reparse";
        return "file";
    }

    std::string getStructuredTimestamp(const FileItem& item) const {
        using namespace std::chrono;
        auto sctp = time_point_cast<system_clock::duration>(
            item.lastWriteTime - fs::file_time_type::clock::now() + system_clock::now()
        );
        std::time_t tt = system_clock::to_time_t(sctp);
        std::tm tmVal;
        localtime_s(&tmVal, &tt);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tmVal);
        return std::string(buf);
    }

    void printStructuredEntries(const std::vector<FileItem>& items) const {
        if (options.outputFormat == "json") {
            std::cout << "[\n";
            for (size_t i = 0; i < items.size(); ++i) {
                const auto& item = items[i];
                std::cout << "  {\n"
                          << "    \"name\": \"" << jsonEscape(item.name) << "\",\n"
                          << "    \"path\": \"" << jsonEscape(pathToUtf8(item.path)) << "\",\n"
                          << "    \"type\": \"" << jsonEscape(getTypeName(item)) << "\",\n"
                          << "    \"size\": " << item.size << ",\n"
                          << "    \"modified\": \"" << jsonEscape(getStructuredTimestamp(item)) << "\",\n"
                          << "    \"attributes\": \"" << jsonEscape(item.getWindowsModeString()) << "\"\n"
                          << "  }" << ((i + 1 < items.size()) ? "," : "") << "\n";
            }
            std::cout << "]\n";
            return;
        }

        if (options.outputFormat == "csv") {
            std::cout << "name,path,type,size,modified,attributes\n";
            for (const auto& item : items) {
                std::cout << csvEscape(item.name) << ','
                          << csvEscape(pathToUtf8(item.path)) << ','
                          << csvEscape(getTypeName(item)) << ','
                          << csvEscape(std::to_string(item.size)) << ','
                          << csvEscape(getStructuredTimestamp(item)) << ','
                          << csvEscape(item.getWindowsModeString()) << "\n";
            }
            return;
        }

        if (options.outputFormat == "table") {
            std::vector<std::pair<std::string, size_t>> widths = {
                {"name", 4}, {"path", 4}, {"type", 4}, {"size", 4}, {"modified", 8}, {"attributes", 10}
            };
            std::vector<std::vector<std::string>> rows;
            rows.reserve(items.size());
            for (const auto& item : items) {
                rows.push_back({
                    item.name,
                    pathToUtf8(item.path),
                    getTypeName(item),
                    std::to_string(item.size),
                    getStructuredTimestamp(item),
                    item.getWindowsModeString()
                });
            }
            for (const auto& row : rows) {
                for (size_t i = 0; i < row.size(); ++i) {
                    widths[i].second = (std::max)(widths[i].second, row[i].length());
                }
            }
            std::cout << std::left;
            for (size_t i = 0; i < widths.size(); ++i) {
                std::cout << std::setw(static_cast<int>(widths[i].second)) << widths[i].first << " | ";
            }
            std::cout << "\n";
            for (const auto& row : rows) {
                for (size_t i = 0; i < row.size(); ++i) {
                    std::cout << std::setw(static_cast<int>(widths[i].second)) << row[i] << " | ";
                }
                std::cout << "\n";
            }
        }
    }

    void sortItems(std::vector<FileItem>& items) const {
        std::sort(items.begin(), items.end(), [&](const FileItem& a, const FileItem& b) {
            if (options.sortByTime) {
                if (a.lastWriteTime != b.lastWriteTime)
                    return options.reverseSort ? (a.lastWriteTime < b.lastWriteTime) 
                                               : (a.lastWriteTime > b.lastWriteTime);
            }
            if (options.sortBySize) {
                if (a.size != b.size)
                    return options.reverseSort ? (a.size < b.size) : (a.size > b.size);
            }
            return options.reverseSort ? (a.name > b.name) : (a.name < b.name);
        });
    }

    std::string decorateName(const FileItem& item) const {
        std::string formattedName = item.name;
        if (options.typeIndicator) {
            if (item.isDirectory) formattedName += "/";
            else if (item.isSymlink) formattedName += "@";
            else if (item.attributes & FILE_ATTRIBUTE_DIRECTORY) formattedName += "/";
            else {
                std::string ext = extensionToUtf8(item.path);
                if (ext == ".exe" || ext == ".bat" || ext == ".cmd") formattedName += "*";
            }
        }

        if (!options.color) {
            return formattedName;
        }

        std::string colorCode = ColorTheme::classify(item.path, item.attributes, item.isSymlink);
        return colorCode + formattedName + ColorTheme::RESET;
    }

    void printLong(const std::vector<FileItem>& items) const {
        uint64_t totalBlocks512 = 0;
        size_t maxOwner = 4, maxDomain = 4, maxSize = 1;

        for (const auto& it : items) {
            totalBlocks512 += (it.size + 511) / 512;
            maxOwner = (std::max)(maxOwner, it.secInfo.owner.length());
            maxDomain = (std::max)(maxDomain, it.secInfo.domain.length());
            maxSize = (std::max)(maxSize, std::to_string(it.size).length());
        }

        std::cout << "total " << totalBlocks512 << "\n";

        for (const auto& it : items) {
            std::cout << ColorTheme::ATTR_COLOR << it.getWindowsModeString() << ColorTheme::RESET << "  "
                      << std::left << std::setw(static_cast<int>(maxDomain)) << it.secInfo.domain << "\\"
                      << std::left << std::setw(static_cast<int>(maxOwner))  << it.secInfo.owner  << "  "
                      << std::right << std::setw(static_cast<int>(maxSize))  << it.size << " "
                      << it.getFormattedTimestamp() << " "
                      << decorateName(it) << "\n";
        }
    }

    void printColumns(const std::vector<FileItem>& items) const {
        if (items.empty()) return;

        int termWidth = getTerminalWidth();
        std::vector<std::string> rendered;
        size_t maxLen = 0;

        for (const auto& it : items) {
            std::string disp = it.name;
            if (options.typeIndicator) {
                if (it.isDirectory) disp += "/";
                else if (it.isSymlink) disp += "@";
                else if (it.path.extension() == ".exe") disp += "*";
            }
            maxLen = (std::max)(maxLen, disp.length());
            rendered.push_back(decorateName(it));
        }

        int colWidth = static_cast<int>(maxLen) + 3;
        int numCols = (std::max)(1, termWidth / colWidth);
        int numRows = static_cast<int>((items.size() + numCols - 1) / numCols);

        for (int r = 0; r < numRows; ++r) {
            for (int c = 0; c < numCols; ++c) {
                size_t idx = c * numRows + r;
                if (idx < items.size()) {
                    std::string plain = items[idx].name;
                    int pad = colWidth - static_cast<int>(plain.length());
                    std::cout << rendered[idx] << std::string((std::max)(1, pad), ' ');
                }
            }
            std::cout << "\n";
        }
    }

public:
    explicit ListingEngine(const ListingOptions& opts) : options(opts) {}

    void listDirectory(const fs::path& dirPath, bool printHeader = false) {
        if (printHeader) {
            std::cout << "\n" << pathToUtf8(dirPath) << ":\n";
        }

        std::vector<FileItem> items;
        std::error_code ec;

        if (options.all) {
            items.emplace_back(dirPath / ".", options.longFormat);
            items.back().name = ".";
            items.emplace_back(dirPath / "..", options.longFormat);
            items.back().name = "..";
        }

        for (const auto& entry : fs::directory_iterator(dirPath, fs::directory_options::skip_permission_denied, ec)) {
            std::string fname = filenameToUtf8(entry.path());

            DWORD attr = GetFileAttributesW(entry.path().c_str());
            bool isHidden = (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_HIDDEN);

            if (!options.all && !options.almostAll) {
                if (isHidden || (fname.length() > 0 && fname[0] == '.')) {
                    continue;
                }
            }

            items.emplace_back(entry.path(), options.longFormat || !options.outputFormat.empty());
        }

        sortItems(items);

        if (!options.outputFormat.empty()) {
            printStructuredEntries(items);
            return;
        }

        if (options.longFormat) {
            printLong(items);
        } else if (options.singleColumn) {
            for (const auto& it : items) {
                std::cout << decorateName(it) << "\n";
            }
        } else {
            printColumns(items);
        }

        if (options.recursive) {
            for (const auto& it : items) {
                if (it.isDirectory && it.name != "." && it.name != "..") {
                    listDirectory(it.path, true);
                }
            }
        }
    }
};

// ============================================================================
// Class: HelpFormatter
// Generates the comprehensive HP-UX Reference Manual page.
// ============================================================================
class HelpFormatter {
public:
    static void printHelp() {
        std::cout << R"(ls(1)               CrossShell for UNIX Reference Manual                 ls(1)

    NAME
        ls - list directory contents with native Windows attributes

    SYNOPSIS
        ls [OPTIONS] [FILE...]

    DESCRIPTION
        The ls command lists information about files and directories. For
        compatibility on Windows NT environments, it displays native Windows
        filesystem attributes without artificial POSIX emulation.

    WINDOWS FILE ATTRIBUTES
        Under long listing format (-l), file modes are formatted as an authentic
        9-character Windows mask:
            Position 1: 'd' = Directory
            Position 2: 'r' = Read-Only
            Position 3: 'a' = Archive
            Position 4: 'h' = Hidden
            Position 5: 's' = System
            Position 6: 'c' = Compressed
            Position 7: 'e' = Encrypted
            Position 8: 'l' = Reparse Point / Symbolic Link
            Position 9: '+' = Discretionary ACL Present

    OPTIONS
        -a, --all
            List all entries including hidden files, system files, and . / ..

        -A, --almost-all
            List all entries including hidden files, omitting . and ..

        -F, --classify
            Append indicator: '/' for directories, '*' for executables, '@' for links.

        -l, --long
            Use long listing format displaying attributes, owner, size, and date.

        -R, --recursive
            Recursively list directory subtrees.

        -r, --reverse
            Reverse the sorting order.

        -t
            Sort entries by modification timestamp (most recent first).

        -S
            Sort entries by file size (largest first).

        -1
            Force single-column output format.

        --no-color
            Disable ANSI color sequences in output.

        --output FORMAT
            Select table, csv, or json output format.

        -h, --help
            Display this reference manual.

        -V, --version
            Display version and license information.

    EXAMPLES
        ls -laF
            List all files with details and type indicators.

        ls -ltr C:\Projects
            List directory sorted by modification time in ascending order.

        ls --json | jq '.[].name'
            Output file list as structured JSON for jq pipeline processing.

    CrossShell for UNIX                                                    ls(1)
)";
    }
};

// ============================================================================
// Class: ArgumentParser
// Parses HP-UX compound parameters (e.g., -latF, -l1S) and file arguments.
// ============================================================================
class ArgumentParser {
public:
    static bool parse(int argc, char* argv[], ListingOptions& options) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--help" || arg == "-h") {
                HelpFormatter::printHelp();
                exit(0);
            }
            if (arg == "-V" || arg == "--version") {
                std::cout << "ls 1.0.0\n";
                exit(0);
            }
            if (arg == "--no-color") {
                options.color = false;
                continue;
            }
            if (arg == "--output") {
                if (i + 1 >= argc) {
                    std::cerr << "ls: missing argument for --output\n";
                    return false;
                }
                options.outputFormat = argv[++i];
                if (options.outputFormat != "json" && options.outputFormat != "csv" && options.outputFormat != "table") {
                    std::cerr << "ls: invalid output format '" << options.outputFormat << "'\n";
                    return false;
                }
                continue;
            }
            if (arg == "--json") {
                options.outputFormat = "json";
                continue;
            }
            if (arg == "--csv") {
                options.outputFormat = "csv";
                continue;
            }
            if (arg == "--table") {
                options.outputFormat = "table";
                continue;
            }

            if (arg.rfind("--", 0) == 0) {
                std::cerr << "ls: unrecognized option '" << arg << "'\nTry 'ls --help' for manual.\n";
                return false;
            }

            if (arg.length() > 1 && arg[0] == '-') {
                for (size_t c = 1; c < arg.length(); ++c) {
                    switch (arg[c]) {
                        case 'a': options.all = true; break;
                        case 'A': options.almostAll = true; break;
                        case 'l': options.longFormat = true; break;
                        case 'F': options.typeIndicator = true; break;
                        case 'R': options.recursive = true; break;
                        case 'r': options.reverseSort = true; break;
                        case 't': options.sortByTime = true; break;
                        case 'S': options.sortBySize = true; break;
                        case '1': options.singleColumn = true; break;
                        default:
                            std::cerr << "ls: illegal option -- " << arg[c] << "\n";
                            std::cerr << "usage: ls [-a | -A] [-l] [-F] [-R] [-r] [-t] [-S] [-1] [file ...]\n";
                            return false;
                    }
                }
            } else {
                options.targets.push_back(arg);
            }
        }

        if (options.targets.empty()) {
            options.targets.push_back(".");
        }

        return true;
    }
};

// ============================================================================
// Main Function
// ============================================================================
int main(int argc, char* argv[]) {
    // Enable ANSI TrueColor / Virtual Terminal Sequences in Windows Console
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            SetConsoleMode(hOut, dwMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }

    ListingOptions options;
    if (!ArgumentParser::parse(argc, argv, options)) {
        return 1;
    }

    ListingEngine engine(options);
    for (size_t i = 0; i < options.targets.size(); ++i) {
        fs::path p(options.targets[i]);
        if (!fs::exists(p)) {
            std::cerr << "ls: " << options.targets[i] << " not found\n";
            continue;
        }

        bool printHeader = options.targets.size() > 1;
        if (fs::is_directory(p) && options.outputFormat.empty()) {
            engine.listDirectory(p, printHeader);
        } else if (!fs::is_directory(p) && options.outputFormat.empty()) {
            std::vector<FileItem> singleItem = { FileItem(p, options.longFormat) };
            if (options.longFormat) {
                // Long list single item
                std::cout << singleItem[0].getWindowsModeString() << " "
                          << singleItem[0].secInfo.domain << "\\" << singleItem[0].secInfo.owner << " "
                          << singleItem[0].size << " "
                          << singleItem[0].getFormattedTimestamp() << " "
                          << singleItem[0].name << "\n";
            } else {
                std::cout << singleItem[0].name << "\n";
            }
        } else {
            std::vector<FileItem> items;
            if (fs::is_directory(p)) {
                std::error_code ec;
                for (const auto& entry : fs::directory_iterator(p, fs::directory_options::skip_permission_denied, ec)) {
                    std::string fname = filenameToUtf8(entry.path());
                    DWORD attr = GetFileAttributesW(entry.path().c_str());
                    bool isHidden = (attr != INVALID_FILE_ATTRIBUTES) && (attr & FILE_ATTRIBUTE_HIDDEN);
                    if (!options.all && !options.almostAll) {
                        if (isHidden || (!fname.empty() && fname[0] == '.')) continue;
                    }
                    items.emplace_back(entry.path(), true);
                }
            } else {
                items.emplace_back(p, true);
            }
            std::sort(items.begin(), items.end(), [&](const FileItem& a, const FileItem& b) {
                return a.name < b.name;
            });
            if (options.outputFormat == "json") {
                std::cout << "[\n";
                for (size_t i = 0; i < items.size(); ++i) {
                    const auto& item = items[i];
                    std::cout << "  {\n"
                              << "    \"name\": \"" << item.name << "\",\n"
                              << "    \"path\": \"" << pathToUtf8(item.path) << "\",\n"
                              << "    \"type\": \"" << (item.isDirectory ? "directory" : "file") << "\",\n"
                              << "    \"size\": " << item.size << ",\n"
                              << "    \"modified\": \"" << item.getFormattedTimestamp() << "\",\n"
                              << "    \"attributes\": \"" << item.getWindowsModeString() << "\"\n"
                              << "  }" << ((i + 1 < items.size()) ? "," : "") << "\n";
                }
                std::cout << "]\n";
            } else if (options.outputFormat == "csv") {
                std::cout << "name,path,type,size,modified,attributes\n";
                for (const auto& item : items) {
                    std::cout << item.name << ',' << pathToUtf8(item.path) << ','
                              << (item.isDirectory ? "directory" : "file") << ','
                              << item.size << ',' << item.getFormattedTimestamp() << ','
                              << item.getWindowsModeString() << "\n";
                }
            } else if (options.outputFormat == "table") {
                std::cout << "NAME | PATH | TYPE | SIZE | MODIFIED | ATTRIBUTES\n";
                for (const auto& item : items) {
                    std::cout << item.name << " | " << pathToUtf8(item.path) << " | "
                              << (item.isDirectory ? "directory" : "file") << " | "
                              << item.size << " | " << item.getFormattedTimestamp() << " | "
                              << item.getWindowsModeString() << "\n";
                }
            }
        }
    }

    return 0;
}