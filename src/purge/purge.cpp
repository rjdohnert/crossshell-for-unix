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
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <filesystem>
#include <regex>
#include <fstream>
#include <chrono>
#include <sstream>
#include <random>
#include <memory>

namespace fs = std::filesystem;

// ============================================================================
// 1. CONSTANTS, DATA MODELS & RAII HANDLES
// ============================================================================

constexpr uint64_t VMS_BLOCK_SIZE = 512;

class ScopedFileHandle {
public:
    explicit ScopedFileHandle(HANDLE handle = INVALID_HANDLE_VALUE) : m_handle(handle) {}

    ~ScopedFileHandle() {
        Close();
    }

    ScopedFileHandle(const ScopedFileHandle&) = delete;
    ScopedFileHandle& operator=(const ScopedFileHandle&) = delete;

    ScopedFileHandle(ScopedFileHandle&& other) noexcept : m_handle(other.m_handle) {
        other.m_handle = INVALID_HANDLE_VALUE;
    }

    ScopedFileHandle& operator=(ScopedFileHandle&& other) noexcept {
        if (this != &other) {
            Close();
            m_handle = other.m_handle;
            other.m_handle = INVALID_HANDLE_VALUE;
        }
        return *this;
    }

    HANDLE Get() const { return m_handle; }
    bool IsValid() const { return m_handle != INVALID_HANDLE_VALUE && m_handle != NULL; }

    void Close() {
        if (m_handle != INVALID_HANDLE_VALUE && m_handle != NULL) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
        }
    }

private:
    HANDLE m_handle;
};

struct FileRecord {
    fs::path fullPath;
    std::wstring baseKey;
    int64_t explicitVersion = -1;
    bool hasExplicitVersion = false;
    fs::file_time_type writeTime;
    uint64_t fileSize = 0;
};

struct PurgeMetrics {
    uint64_t totalDeleted = 0;
    uint64_t totalBytesFreed = 0;
};

// ============================================================================
// 2. STRING & TIME FORMATTING UTILITIES
// ============================================================================

class StringHelper {
public:
    static std::string ToUpper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return s;
    }

    static std::wstring ToUpperW(std::wstring s) {
        std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return static_cast<wchar_t>(::towupper(c)); });
        return s;
    }

    static std::string WStringToString(const std::wstring& wstr) {
        if (wstr.empty()) return "";
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
        std::string strTo(size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &strTo[0], size_needed, NULL, NULL);
        return strTo;
    }

    static std::wstring StringToWString(const std::string& str) {
        if (str.empty()) return L"";
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), NULL, 0);
        std::wstring wstrTo(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, &str[0], static_cast<int>(str.size()), &wstrTo[0], size_needed);
        return wstrTo;
    }

    static std::string FormatBytes(uint64_t bytes) {
        uint64_t blocks = (bytes + VMS_BLOCK_SIZE - 1) / VMS_BLOCK_SIZE;
        const char* units[] = { "B", "KB", "MB", "GB", "TB" };
        int unitIndex = 0;
        double count = static_cast<double>(bytes);

        while (count >= 1024.0 && unitIndex < 4) {
            count /= 1024.0;
            unitIndex++;
        }

        std::ostringstream ss;
        ss << blocks << " block" << (blocks == 1 ? "" : "s") << " / ";
        if (unitIndex == 0) {
            ss << static_cast<uint64_t>(count) << " " << units[unitIndex];
        } else {
            ss << std::fixed << std::setprecision(1) << count << " " << units[unitIndex];
        }
        return ss.str();
    }

    static bool WildcardMatch(const std::wstring& pattern, const std::wstring& text) {
        if (pattern.empty() || pattern == L"*") return true;
        std::wstring regStr = L"^";
        for (wchar_t c : pattern) {
            if (c == L'*') regStr += L".*";
            else if (c == L'?') regStr += L".";
            else if (wcschr(L"\\.+^$()[]{}|", c)) { regStr += L"\\"; regStr += c; }
            else regStr += c;
        }
        regStr += L"$";
        std::wregex rx(regStr, std::regex::icase);
        return std::regex_match(text, rx);
    }
};

// ============================================================================
// 3. SECURE FILE ERASER
// ============================================================================

class SecureEraser {
public:
    static bool EraseFile(const fs::path& filePath, uint64_t fileSize) {
        ScopedFileHandle hFile(CreateFileW(
            filePath.c_str(),
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_WRITE_THROUGH | FILE_FLAG_NO_BUFFERING,
            NULL
        ));

        if (!hFile.IsValid()) {
            return false;
        }

        if (fileSize > 0) {
            std::vector<char> buffer(65536, 0);
            std::mt19937_64 rng(1337);
            std::uniform_int_distribution<uint64_t> dist(0, 0xFFFFFFFFFFFFFFFF);

            // Pass 1: Random Bitmask
            for (size_t i = 0; i < buffer.size(); i += 8) {
                uint64_t r = dist(rng);
                memcpy(&buffer[i], &r, sizeof(uint64_t));
            }

            uint64_t writtenTotal = 0;
            while (writtenTotal < fileSize) {
                DWORD toWrite = static_cast<DWORD>(std::min<uint64_t>(buffer.size(), fileSize - writtenTotal));
                DWORD written = 0;
                if (!WriteFile(hFile.Get(), buffer.data(), toWrite, &written, NULL) || written == 0) break;
                writtenTotal += written;
            }

            // Pass 2: Cryptographic Zero Fill
            SetFilePointer(hFile.Get(), 0, NULL, FILE_BEGIN);
            std::fill(buffer.begin(), buffer.end(), 0x00);
            writtenTotal = 0;
            while (writtenTotal < fileSize) {
                DWORD toWrite = static_cast<DWORD>(std::min<uint64_t>(buffer.size(), fileSize - writtenTotal));
                DWORD written = 0;
                if (!WriteFile(hFile.Get(), buffer.data(), toWrite, &written, NULL) || written == 0) break;
                writtenTotal += written;
            }
            FlushFileBuffers(hFile.Get());
        }

        return true;
    }
};

// ============================================================================
// 4. FILE VERSIONING EXTRACTION & OPTIONS
// ============================================================================

class FileVersionExtractor {
public:
    static FileRecord Parse(const fs::directory_entry& entry) {
        FileRecord rec;
        rec.fullPath = entry.path();
        rec.fileSize = entry.file_size();
        rec.writeTime = entry.last_write_time();

        std::wstring filename = entry.path().filename().wstring();

        static const std::wregex vmsRegex(LR"((.+);(\d+)$)", std::regex::icase);
        static const std::wregex numExtRegex(LR"((.+)\.(\d+)$)", std::regex::icase);
        static const std::wregex vTagRegex(LR"((.*?)[._]v(\d+)(\.[^.]*)?$)", std::regex::icase);

        std::wsmatch match;
        if (std::regex_match(filename, match, vmsRegex)) {
            rec.baseKey = StringHelper::ToUpperW(match[1].str());
            rec.explicitVersion = std::stoll(match[2].str());
            rec.hasExplicitVersion = true;
        } else if (std::regex_match(filename, match, numExtRegex)) {
            rec.baseKey = StringHelper::ToUpperW(match[1].str());
            rec.explicitVersion = std::stoll(match[2].str());
            rec.hasExplicitVersion = true;
        } else if (std::regex_match(filename, match, vTagRegex)) {
            rec.baseKey = StringHelper::ToUpperW(match[1].str() + match[3].str());
            rec.explicitVersion = std::stoll(match[2].str());
            rec.hasExplicitVersion = true;
        } else {
            rec.baseKey = StringHelper::ToUpperW(filename);
            rec.explicitVersion = -1;
            rec.hasExplicitVersion = false;
        }

        return rec;
    }
};

class PurgeOptions {
public:
    int keepCount = 1;
    bool log = false;
    bool confirm = false;
    bool erase = false;
    bool grandTotal = false;
    bool help = false;
    std::string helpTopic = "";
    std::wstring excludePattern = L"";
    std::wstring outputSpec = L"";
    std::wstring fileSpec = L"";

    void Parse(int argc, char* argv[]) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            // 1. POSIX Double-Dash Options (--keep=2, --log, etc.)
            if (arg.rfind("--", 0) == 0) {
                std::string opt = StringHelper::ToUpper(arg.substr(2));
                if (opt.rfind("KEEP=", 0) == 0) {
                    keepCount = std::max(1, std::stoi(opt.substr(5)));
                } else if (opt == "LOG") {
                    log = true;
                } else if (opt == "CONFIRM") {
                    confirm = true;
                } else if (opt == "ERASE") {
                    erase = true;
                } else if (opt == "GRAND-TOTAL" || opt == "GRAND_TOTAL") {
                    grandTotal = true;
                } else if (opt.rfind("EXCLUDE=", 0) == 0) {
                    excludePattern = StringHelper::StringToWString(arg.substr(10));
                } else if (opt.rfind("OUTPUT=", 0) == 0) {
                    outputSpec = StringHelper::StringToWString(arg.substr(9));
                } else if (opt == "HELP") {
                    help = true;
                    if (i + 1 < argc && argv[i + 1][0] != '-' && argv[i + 1][0] != '/') {
                        helpTopic = StringHelper::ToUpper(argv[++i]);
                    }
                } else {
                    std::cerr << "%DCL-W-IVQUAL, unrecognized switch --" << opt << "\n";
                }
            }
            // 2. VMS & CMD Qualifiers (/KEEP=2, /LOG, /CONFIRM, /?)
            else if (arg[0] == '/') {
                std::string q = StringHelper::ToUpper(arg.substr(1));
                if (q.rfind("KEEP=", 0) == 0) {
                    keepCount = std::max(1, std::stoi(q.substr(5)));
                } else if (q.rfind("K:", 0) == 0) {
                    keepCount = std::max(1, std::stoi(q.substr(2)));
                } else if (q == "LOG" || q == "L") {
                    log = true;
                } else if (q == "CONFIRM" || q == "C") {
                    confirm = true;
                } else if (q == "ERASE" || q == "E") {
                    erase = true;
                } else if (q == "GRAND_TOTAL" || q == "GRANDTOTAL" || q == "G") {
                    grandTotal = true;
                } else if (q.rfind("EXCLUDE=", 0) == 0) {
                    excludePattern = StringHelper::StringToWString(arg.substr(9));
                } else if (q.rfind("OUTPUT=", 0) == 0) {
                    outputSpec = StringHelper::StringToWString(arg.substr(8));
                } else if (q == "HELP" || q == "?" || q == "H") {
                    help = true;
                    if (i + 1 < argc && argv[i + 1][0] != '/' && argv[i + 1][0] != '-') {
                        helpTopic = StringHelper::ToUpper(argv[++i]);
                    }
                } else {
                    std::cerr << "%DCL-W-IVQUAL, unrecognized qualifier /" << q << "\n";
                }
            }
            // 3. POSIX Short Options (-k 2, -l, -c, -e, -g, -x pat)
            else if (arg[0] == '-') {
                std::string s = arg.substr(1);
                if (s == "k" && i + 1 < argc) {
                    keepCount = std::max(1, std::stoi(argv[++i]));
                } else if (s == "l") {
                    log = true;
                } else if (s == "c") {
                    confirm = true;
                } else if (s == "e") {
                    erase = true;
                } else if (s == "g") {
                    grandTotal = true;
                } else if (s == "x" && i + 1 < argc) {
                    excludePattern = StringHelper::StringToWString(argv[++i]);
                } else if (s == "o" && i + 1 < argc) {
                    outputSpec = StringHelper::StringToWString(argv[++i]);
                } else if (s == "h" || s == "?") {
                    help = true;
                    if (i + 1 < argc && argv[i + 1][0] != '-' && argv[i + 1][0] != '/') {
                        helpTopic = StringHelper::ToUpper(argv[++i]);
                    }
                } else {
                    std::cerr << "%DCL-W-IVQUAL, unrecognized option -" << s << "\n";
                }
            }
            // 4. Positional Argument (Filespec or HELP verb)
            else {
                std::string p = StringHelper::ToUpper(arg);
                if (p == "HELP") {
                    help = true;
                    if (i + 1 < argc) {
                        helpTopic = StringHelper::ToUpper(argv[++i]);
                    }
                } else {
                    fileSpec = StringHelper::StringToWString(arg);
                }
            }
        }
    }
};

// ============================================================================
// 5. REPORTER & PURGE ENGINE
// ============================================================================

class PurgeReporter {
public:
    static void DisplayHelp(const std::string& topic, std::ostream& out) {
           out << R"HELP(purge(1)                CrossShell for UNIX Reference Manual                   purge(1)

    NAME
        purge - remove older versions of files

    SYNOPSIS
        purge [FILESPEC] [OPTIONS]
        purge [HELP_TOPIC]

    DESCRIPTION
        Groups matching files by VMS-style suffixes, numeric extensions, version
        tags, or timestamps and deletes older revisions. With no filespec, the
        current directory is scanned using '*'.

    OPTIONS
        /KEEP=N, /K:N, -k N, --keep=N
            Retain the newest N versions; the default is 1.
        /LOG, /L, -l, --log
            Report deleted files and sizes.
        /CONFIRM, /C, -c, --confirm
            Prompt before each deletion.
        /ERASE, /E, -e, --erase
            Overwrite with random data and zeros before deletion.
        /EXCLUDE=PATTERN, -x PATTERN, --exclude=PATTERN
            Skip files matching PATTERN.
        /GRAND_TOTAL, /G, -g, --grand-total
            Display a cumulative deletion summary.
        /OUTPUT=FILE, -o FILE, --output=FILE
            Redirect report output to FILE.
        /HELP [TOPIC], /?, -h, --help [TOPIC]
            Display general or topic-specific help.

    HELP TOPICS
        QUALIFIERS, PARAMETERS, DESCRIPTION, EXAMPLES

    EXAMPLES
        purge /KEEP=2 /LOG
        purge *.bak -k 1 --erase --log
        purge /CONFIRM /EXCLUDE=*.SYS

    EXIT STATUS
        0          No matching files or completed processing.
        1          Help or normal purge completion as implemented.
        2          Output-file or directory failure.
        4          Directory enumeration failure.

    CrossShell for UNIX                                                       purge(1)
    )HELP";
           return;

        std::string t = StringHelper::ToUpper(topic);

        if (t.empty()) {
            out << "\n purge v6.0.0 \n\n"
                << "  Deletes previous versions of files. On Windows systems with non-versioning\n"
                << "  file structures, purge groups matching files and purges older revisions\n"
                << "  based on version tags, numeric extensions, or timestamp histories.\n\n"
                << "  Format\n\n"
                << "       purge [filespec] [/qualifiers]\n"
                << "       purge [filespec] [options]\n\n"
                << "  Command Qualifiers (VMS / CMD / POSIX)            Defaults\n\n"
                << "       /KEEP=n, /K:n, -k <n>, --keep=<n>           /KEEP=1\n"
                << "       /LOG, /L, -l, --log                         /NOLOG\n"
                << "       /CONFIRM, /C, -c, --confirm                 /NOCONFIRM\n"
                << "       /ERASE, /E, -e, --erase                     /NOERASE\n"
                << "       /EXCLUDE=pattern, -x <pat>, --exclude=<pat> None\n"
                << "       /GRAND_TOTAL, /G, -g, --grand-total         /NOGRAND_TOTAL\n"
                << "       /OUTPUT=filespec, -o <file>, --output=<file> \n"
                << "       /HELP [topic], /?, -h, --help               Display help\n\n"
                << "  Prompts\n\n"
                << "       File: [filespec]\n\n"
                << "  Additional information available:\n\n"
                << "       Parameters   Description   Qualifiers   Examples\n\n";
        } else if (t == "QUALIFIERS" || t == "/QUALIFIERS") {
            out << "\n purge\n\n"
                << "  Qualifiers (VMS / Windows CMD / POSIX)\n\n"
                << "    /KEEP=number | -k <n> | --keep=<n>\n"
                << "       Specifies the number of latest versions of a file to retain.\n"
                << "       The default is 1 (retains only the highest/latest file).\n\n"
                << "    /LOG | -l | --log\n"
                << "       Controls whether the utility displays informational messages\n"
                << "       showing the file name and size for each file purged.\n\n"
                << "    /CONFIRM | -c | --confirm\n"
                << "       Controls whether PURGE prompts you for confirmation before deleting\n"
                << "       each file: DELETE filespec? [N]:\n\n"
                << "    /ERASE | -e | --erase\n"
                << "       Overwrites file storage locations with a multi-pass secure random and\n"
                << "       zero pattern prior to unlinking the file from the filesystem.\n\n"
                << "    /EXCLUDE=pattern | -x <pattern> | --exclude=<pattern>\n"
                << "       Excludes files that match the specified wildcard pattern.\n\n"
                << "    /GRAND_TOTAL | -g | --grand-total\n"
                << "       Displays a cumulative summary of the total files deleted and space freed.\n\n"
                << "    /OUTPUT=filespec | -o <file> | --output=<file>\n"
                << "       Directs the output to the specified file rather than SYS$OUTPUT.\n\n";
        } else if (t == "PARAMETERS") {
            out << "\n purge\n\n"
                << "  Parameters\n\n"
                << "    filespec\n"
                << "       Specifies the file or files to be purged. Wildcard characters (* and ?)\n"
                << "       are accepted. If no parameter is supplied, *.* is assumed in the current\n"
                << "       default directory.\n\n";
        } else if (t == "DESCRIPTION") {
            out << "\n purge\n\n"
                << "  Description\n\n"
                << "       The OpenVMS PURGE clone inspects directories and automatically detects\n"
                << "       three primary forms of versioning:\n"
                << "       1. Explicit Suffixes :  REPORT.TXT;1, REPORT.TXT;2\n"
                << "       2. Backup Notation   :  BACKUP.BAK.1, BACKUP.BAK.2\n"
                << "       3. Modification-Time :  General files grouped by base stem and sorted\n"
                << "                               by NTFS LastWriteTime.\n\n";
        } else if (t == "EXAMPLES") {
            out << "\n purge\n\n"
                << "  Examples\n\n"
                << "    $ purge /KEEP=2 /LOG\n"
                << "       Deletes all but the two latest versions of every file in the directory.\n\n"
                << "    $ purge *.bak -k 1 --erase --log\n"
                << "       POSIX-style invocation: securely wipes and purges old .bak archives.\n\n"
                << "    $ purge /CONFIRM /EXCLUDE=*.SYS\n"
                << "       Interactively prompts before purging each file, excluding system files.\n\n";
        } else {
            out << "\n%HELP-W-NOTOPIC, No help available for \"" << topic << "\"\n\n";
        }
    }
};

class PurgeEngine {
public:
    static int Execute(const PurgeOptions& opts, std::ostream& outStream) {
        fs::path searchDir = ".";
        std::wstring matchPattern = L"*";

        if (!opts.fileSpec.empty()) {
            fs::path p(opts.fileSpec);
            if (fs::is_directory(p)) {
                searchDir = p;
                matchPattern = L"*";
            } else {
                if (p.has_parent_path()) searchDir = p.parent_path();
                if (p.has_filename()) matchPattern = p.filename().wstring();
            }
        }

        if (!fs::exists(searchDir)) {
            outStream << "%RMS-E-DNF, directory not found " << searchDir.string() << "\n";
            return 2;
        }

        std::map<std::wstring, std::vector<FileRecord>> groups;

        try {
            for (const auto& entry : fs::directory_iterator(searchDir)) {
                if (!entry.is_regular_file()) continue;

                std::wstring filename = entry.path().filename().wstring();

                if (!StringHelper::WildcardMatch(matchPattern, filename)) continue;

                if (!opts.excludePattern.empty() && StringHelper::WildcardMatch(opts.excludePattern, filename)) {
                    continue;
                }

                FileRecord rec = FileVersionExtractor::Parse(entry);
                groups[rec.baseKey].push_back(rec);
            }
        } catch (const std::exception& ex) {
            outStream << "%RMS-F-SYS, error accessing directory: " << ex.what() << "\n";
            return 4;
        }

        if (groups.empty()) {
            outStream << "%PURGE-W-NOFILES, no matching files found\n";
            return 0;
        }

        PurgeMetrics metrics;

        for (auto& [key, records] : groups) {
            if (records.size() <= static_cast<size_t>(opts.keepCount)) {
                continue;
            }

            std::sort(records.begin(), records.end(), [](const FileRecord& a, const FileRecord& b) {
                if (a.hasExplicitVersion && b.hasExplicitVersion) {
                    return a.explicitVersion < b.explicitVersion;
                }
                return a.writeTime < b.writeTime;
            });

            size_t toDeleteCount = records.size() - static_cast<size_t>(opts.keepCount);

            for (size_t i = 0; i < toDeleteCount; ++i) {
                const auto& fileRec = records[i];
                bool proceed = true;

                if (opts.confirm) {
                    std::cout << "DELETE " << fileRec.fullPath.string() << "? [N]: ";
                    std::string response;
                    if (!std::getline(std::cin, response) || response.empty()) {
                        proceed = false;
                    } else {
                        char c = static_cast<char>(std::toupper(response[0]));
                        proceed = (c == 'Y' || c == 'T' || c == '1');
                    }
                }

                if (!proceed) continue;

                DWORD attrs = GetFileAttributesW(fileRec.fullPath.c_str());
                if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_READONLY)) {
                    SetFileAttributesW(fileRec.fullPath.c_str(), attrs & ~FILE_ATTRIBUTE_READONLY);
                }

                if (opts.erase) {
                    SecureEraser::EraseFile(fileRec.fullPath, fileRec.fileSize);
                }

                std::error_code ec;
                if (fs::remove(fileRec.fullPath, ec)) {
                    metrics.totalDeleted++;
                    metrics.totalBytesFreed += fileRec.fileSize;

                    if (opts.log) {
                        outStream << "%PURGE-I-FILPURG, " 
                                  << fileRec.fullPath.string() 
                                  << " deleted (" << StringHelper::FormatBytes(fileRec.fileSize) << ")\n";
                    }
                } else {
                    if (ec.value() == ERROR_ACCESS_DENIED) {
                        outStream << "%RMS-F-PRV, privilege violation deleting " << fileRec.fullPath.string() << "\n";
                    } else if (ec.value() == ERROR_SHARING_VIOLATION || ec.value() == ERROR_LOCK_VIOLATION) {
                        outStream << "%RMS-E-FLK, file locked by another process " << fileRec.fullPath.string() << "\n";
                    } else {
                        outStream << "%RMS-E-DEL, error deleting file " << fileRec.fullPath.string() << "\n";
                    }
                }
            }
        }

        if (opts.log || opts.grandTotal || metrics.totalDeleted > 0) {
            outStream << "%PURGE-I-TOTAL, " << metrics.totalDeleted << " file" << (metrics.totalDeleted == 1 ? "" : "s")
                      << " deleted (" << StringHelper::FormatBytes(metrics.totalBytesFreed) << " freed)\n";
        }

        return 1;
    }
};

// ============================================================================
// 6. APPLICATION CONTROLLER
// ============================================================================

class PurgeApplication {
public:
    int Run(int argc, char* argv[]) const {
        PurgeOptions opts;
        opts.Parse(argc, argv);

        std::ofstream fileStream;
        std::ostream* outStream = &std::cout;

        if (!opts.outputSpec.empty()) {
            fileStream.open(opts.outputSpec, std::ios::out);
            if (!fileStream.is_open()) {
                std::cerr << "%RMS-F-CRE, error creating output file " << StringHelper::WStringToString(opts.outputSpec) << "\n";
                return 2;
            }
            outStream = &fileStream;
        }

        if (opts.help) {
            PurgeReporter::DisplayHelp(opts.helpTopic, *outStream);
            return 1;
        }

        return PurgeEngine::Execute(opts, *outStream);
    }
};

int main(int argc, char* argv[]) {
    PurgeApplication app;
    return app.Run(argc, argv);
}